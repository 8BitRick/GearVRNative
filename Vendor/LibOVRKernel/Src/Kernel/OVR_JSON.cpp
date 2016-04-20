/************************************************************************************

PublicHeader:   None
Filename    :   OVR_JSON.h
Content     :   JSON format reader and writer
Created     :   April 9, 2013
Author      :   Brant Lewis
Notes       :
  The code is a derivative of the cJSON library written by Dave Gamble and subject 
  to the following permissive copyright.

  Copyright (c) 2009 Dave Gamble
 
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:
 
  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.
 
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.


Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>
#include "OVR_JSON.h"
#include "OVR_SysFile.h"
#include "OVR_Log.h"
#include "OVR_Math.h"
#include "OVR_String_Utils.h"

namespace OVR {


//-----------------------------------------------------------------------------
// Create a new copy of a string
static char* JSON_strdup(const char* str)
{
    size_t len  = OVR_strlen(str) + 1;
    char* copy = (char*)OVR_ALLOC(len);
    if (!copy)
        return 0;
    memcpy(copy, str, len);
    return copy;
}


//-----------------------------------------------------------------------------
// Render the number from the given item into a string.
static char* PrintNumber(double d)
{
    char *str;
    int valueint = (int)d;

    if (fabs(((double)valueint)-d) <= DBL_EPSILON && d <= INT_MAX && d >= INT_MIN)
    {
        str=(char*)OVR_ALLOC(21);   // 2^64+1 can be represented in 21 chars.
        if (str)
            OVR_sprintf(str, 21, "%d", valueint);
    }
    else
    {
        str=(char*)OVR_ALLOC(64);    // This is a nice tradeoff.
        if (str)
        {
            // The JSON Standard, section 7.8.3, specifies that decimals are always expressed with '.' and 
            // not some locale-specific decimal such as ',' or ' '. However, since we are using the C standard
            // library below to write a floating point number, we need to make sure that it's writing a '.' 
            // and not something else. We can't change the locale (even temporarily) here, as it will affect 
            // the whole process by default. That are compiler-specific ways to change this per-thread, but 
            // below we implement the simple solution of simply fixing the decimal after the string was written.

            if (fabs(floor(d)-d) <= DBL_EPSILON && fabs(d) < 1.0e60)
                OVR_sprintf(str, 64, "%.0f", d);
            else if (fabs(d) < 1.0e-6 || fabs(d) > 1.0e9)
                OVR_sprintf(str, 64, "%e", d);
            else
                OVR_sprintf(str, 64, "%f", d);
        }
    }
    return str;
}


// Parse the input text into an un-escaped cstring, and populate item.
static const unsigned char firstByteMark[7] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };

// Helper to assign error sting and return 0.
const char* AssignError(const char** perror, const char *errorMessage)
{
    if (perror)
        *perror = errorMessage;
    return 0;
}

//-----------------------------------------------------------------------------
// ***** JSON Node class

JSON::JSON(JSONItemType itemType) :
    Type(itemType), dValue(0.0)
{
}

JSON::~JSON()
{
    JSON* child = Children.GetFirst();
    while (!Children.IsNull(child))
    {
        child->RemoveNode();
        child->Release();
        child = Children.GetFirst();
    }
}

//-----------------------------------------------------------------------------
// Parse the input text to generate a number, and populate the result into item
// Returns the text position after the parsed number
const char* JSON::parseNumber(const char *num)
{
    const char* num_start = num;
    double      n=0, sign=1, scale=0;
    int         subscale     = 0,
                signsubscale = 1;

    // Could use sscanf for this?
    if (*num == '-')
    {
        sign=-1,num++;    // Has sign?
    }
    if (*num == '0')
    {
        num++;            // is zero
    }

    if (*num>='1' && *num<='9') 
    {
        do
        {
            n = (n*10.0) + (*num++ - '0');
        }
        while (*num>='0' && *num<='9');    // Number?
    }

    if (*num=='.' && num[1]>='0' && num[1]<='9')
    {
        num++;
        do
        {
            n=(n*10.0)+(*num++ -'0');
            scale--;
        }
        while (*num>='0' && *num<='9');  // Fractional part?
    }

    if (*num=='e' || *num=='E')        // Exponent?
    {
        num++;
        if (*num == '+')
        {
            num++;
        }
        else if (*num=='-')
        {
            signsubscale=-1;
            num++;        // With sign?
        }

        while (*num >= '0' && *num <= '9')
        {
            subscale = (subscale * 10) + (*num++ - '0');    // Number?
        }
    }

    // Number = +/- number.fraction * 10^+/- exponent
    n = sign*n*pow(10.0, (scale + subscale*signsubscale));

    // Assign parsed value.
    Type = JSON_Number;
    dValue = n;
    Value.AssignString(num_start, num - num_start);

    return num;
}

// Parses a hex string up to the specified number of digits.
// Returns the first character after the string.
const char* ParseHex(unsigned* val, unsigned digits, const char* str)
{
    *val = 0;

    for(unsigned digitCount = 0; digitCount < digits; digitCount++, str++)
    {
        unsigned v = *str;

        if ((v >= '0') && (v <= '9'))
            v -= '0';
        else if ((v >= 'a') && (v <= 'f'))
            v = 10 + v - 'a';
        else if ((v >= 'A') && (v <= 'F'))
            v = 10 + v - 'A';
        else
            break;

        *val = *val * 16 + v;
    }

    return str;
}

//-----------------------------------------------------------------------------
// Parses the input text into a string item and returns the text position after
// the parsed string
const char* JSON::parseString(const char* str, const char** perror)
{
    const char* ptr = str+1;
    const char* p;
    char*       ptr2;
    char*       out;
    int         len=0;
    unsigned    uc, uc2;
    
    if (*str!='\"')
    {
        return AssignError(perror, "Syntax Error: Missing quote");
    }
    
    while (*ptr!='\"' && *ptr && ++len)
    {   
        if (*ptr++ == '\\') ptr++;    // Skip escaped quotes.
    }
    
    // This is how long we need for the string, roughly.
    out=(char*)OVR_ALLOC(len+1);
    if (!out)
        return 0;
    
    ptr = str+1;
    ptr2= out;

    while (*ptr!='\"' && *ptr)
    {
        if (*ptr!='\\')
        {
            *ptr2++ = *ptr++;
        }
        else
        {
            ptr++;
            switch (*ptr)
            {
                case 'b': *ptr2++ = '\b';    break;
                case 'f': *ptr2++ = '\f';    break;
                case 'n': *ptr2++ = '\n';    break;
                case 'r': *ptr2++ = '\r';    break;
                case 't': *ptr2++ = '\t';    break;

                // Transcode utf16 to utf8.
                case 'u':

                    // Get the unicode char.
                    p = ParseHex(&uc, 4, ptr + 1);
                    if (ptr != p)
                        ptr = p - 1;

                    if ((uc>=0xDC00 && uc<=0xDFFF) || uc==0)
                        break;    // Check for invalid.

                    // UTF16 surrogate pairs.
                    if (uc>=0xD800 && uc<=0xDBFF)
                    {
                        if (ptr[1]!='\\' || ptr[2]!='u')
                            break;    // Missing second-half of surrogate.

                        p= ParseHex(&uc2, 4, ptr + 3);
                        if (ptr != p)
                            ptr = p - 1;
                        
                        if (uc2<0xDC00 || uc2>0xDFFF)
                            break;    // Invalid second-half of surrogate.

                        uc = 0x10000 + (((uc&0x3FF)<<10) | (uc2&0x3FF));
                    }

                    len=4;
                    
                    if (uc<0x80)
                        len=1;
                    else if (uc<0x800)
                        len=2;
                    else if (uc<0x10000)
                        len=3;
                    
                    ptr2+=len;
                    
                    switch (len)
                    {
                        case 4: *--ptr2 =((uc | 0x80) & 0xBF); uc >>= 6;
                            //no break, fall through
                        case 3: *--ptr2 =((uc | 0x80) & 0xBF); uc >>= 6;
                            //no break
                        case 2: *--ptr2 =((uc | 0x80) & 0xBF); uc >>= 6;
                            //no break
                        case 1: *--ptr2 = (char)(uc | firstByteMark[len]);
                            //no break
                    }
                    ptr2+=len;
                    break;

                default:
                    *ptr2++ = *ptr;
                    break;
            }
            ptr++;
        }
    }

    *ptr2 = 0;
    if (*ptr=='\"')
        ptr++;
    
    // Make a copy of the string 
    Value=out;
    OVR_FREE(out);
    Type=JSON_String;

    return ptr;
}

//-----------------------------------------------------------------------------
// Render the string provided to an escaped version that can be printed.
char* PrintString(const char* str)
{
    const char *ptr;
    char *ptr2,*out;
    int len=0;
    unsigned char token;
    
    if (!str)
        return JSON_strdup("");
    ptr=str;
    
    token=*ptr;
    while (token && ++len)\
    {
        if (strchr("\"\\\b\f\n\r\t",token))
            len++;
        else if (token<32) 
            len+=5;
        ptr++;
        token=*ptr;
    }
    
    int buff_size = len+3;
    out=(char*)OVR_ALLOC(buff_size);
    if (!out)
        return 0;

    ptr2 = out;
    ptr  = str;
    *ptr2++ = '\"';

    while (*ptr)
    {
        if ((unsigned char)*ptr>31 && *ptr!='\"' && *ptr!='\\') 
            *ptr2++=*ptr++;
        else
        {
            *ptr2++='\\';
            switch (token=*ptr++)
            {
                case '\\':    *ptr2++='\\';    break;
                case '\"':    *ptr2++='\"';    break;
                case '\b':    *ptr2++='b';    break;
                case '\f':    *ptr2++='f';    break;
                case '\n':    *ptr2++='n';    break;
                case '\r':    *ptr2++='r';    break;
                case '\t':    *ptr2++='t';    break;
                default: 
                    OVR_sprintf(ptr2, buff_size - (ptr2-out), "u%04x",token);
                    ptr2+=5;
                    break;    // Escape and print.
            }
        }
    }
    *ptr2++='\"';
    *ptr2++='\0';
    return out;
}

//-----------------------------------------------------------------------------
// Utility to jump whitespace and cr/lf
static const char* skip(const char* in)
{
    while (in && *in && (unsigned char)*in<=' ') 
        in++; 
    return in;
}

//-----------------------------------------------------------------------------
// Parses the supplied buffer of JSON text and returns a JSON object tree
// The returned object must be Released after use
JSON* JSON::Parse(const char* buff, const char** perror)
{
    const char* end = 0;
    JSON*       json = new JSON();
    
    if (!json)
    {
        AssignError(perror, "Error: Failed to allocate memory");
        return 0;
    }
 
    end = json->parseValue(skip(buff), perror);
    if (!end)
    {
        json->Release();
        return NULL;
    }    // parse failure. ep is set.

    return json;
}

//-----------------------------------------------------------------------------
// Parser core - when encountering text, process appropriately.
const char* JSON::parseValue(const char* buff, const char** perror)
{
    if (perror)
        *perror = 0;

    if (!buff)
        return NULL;    // Fail on null.

    if (!strncmp(buff, "null", 4))
    {
        Type = JSON_Null;
        return buff + 4;
    }
    if (!strncmp(buff, "false", 5))
    { 
        Type   = JSON_Bool;
        Value  = "false";
        dValue = 0.0;
        return buff + 5;
    }
    if (!strncmp(buff, "true", 4))
    {
        Type   = JSON_Bool;
        Value  = "true";
        dValue = 1.0;
        return buff + 4;
    }
    if (*buff=='\"')
    {
        return parseString(buff, perror);
    }
    if (*buff=='-' || (*buff>='0' && *buff<='9'))
    { 
        return parseNumber(buff);
    }
    if (*buff=='[')
    { 
        return parseArray(buff, perror);
    }
    if (*buff=='{')
    {
        return parseObject(buff, perror);
    }

    return AssignError(perror, StringUtils::Va( "Syntax Error: Invalid syntax: '%s'", buff) );
}

//-----------------------------------------------------------------------------
// Render a value to text. 
char* JSON::PrintValue(int depth, bool fmt)
{
    char *out=0;

    switch (Type)
    {
        case JSON_Null:        out = JSON_strdup("null");    break;
        case JSON_Bool:
            if (dValue == 0)
                out = JSON_strdup("false");
            else
                out = JSON_strdup("true");
            break;
        case JSON_Number:    out = PrintNumber(dValue); break;
        case JSON_String:    out = PrintString(Value.ToCStr()); break;
        case JSON_Array:    out = PrintArray(depth, fmt); break;
        case JSON_Object:    out = PrintObject(depth, fmt); break;
        case JSON_None: OVR_ASSERT_LOG(false, ("Bad JSON type.")); break;
    }
    return out;
}

//-----------------------------------------------------------------------------
// Build an array object from input text and returns the text position after
// the parsed array
const char* JSON::parseArray(const char* buff, const char** perror)
{
    JSON *child;
    if (*buff!='[')
    {
        return AssignError(perror, "Syntax Error: Missing opening bracket");
    }

    Type=JSON_Array;
    buff=skip(buff+1);
    
    if (*buff==']')
        return buff+1;    // empty array.

    child = new JSON();
    if (!child)
        return 0;         // memory fail
    Children.PushBack(child);
    
    buff=skip(child->parseValue(skip(buff), perror));    // skip any spacing, get the buff. 
    if (!buff)
        return 0;

    while (*buff==',')
    {
        JSON *new_item = new JSON();
        if (!new_item)
            return AssignError(perror, "Error: Failed to allocate memory");
        
        Children.PushBack(new_item);

        buff=skip(new_item->parseValue(skip(buff+1), perror));
        if (!buff)
            return AssignError(perror, "Error: Failed to allocate memory");
    }

    if (*buff==']')
        return buff+1;    // end of array

    return AssignError(perror, "Syntax Error: Missing ending bracket");
}

//-----------------------------------------------------------------------------
// Render an array to text.  The returned text must be freed
char* JSON::PrintArray(int depth, bool fmt)
{
    char **  entries;
    char *   out = 0, *ptr,*ret;
    intptr_t len = 5;
    
    bool fail = false;
    
    // How many entries in the array? 
    int numentries = GetItemCount();
    if (!numentries)
    {
        out=(char*)OVR_ALLOC(3);
        if (out)
            OVR_strcpy(out, 3, "[]");
        return out;
    }
    // Allocate an array to hold the values for each
    entries=(char**)OVR_ALLOC(numentries*sizeof(char*));
    if (!entries)
        return 0;
    memset(entries,0,numentries*sizeof(char*));

    //// Retrieve all the results:
    JSON* child = Children.GetFirst();
    for (int i=0; i<numentries; i++)
    {
        //JSON* child = Children[i];
        ret=child->PrintValue(depth+1, fmt);
        entries[i]=ret;
        if (ret)
            len+=OVR_strlen(ret)+2+(fmt?1:0);
        else
        {
            fail = true;
            break;
        }
        child = Children.GetNext(child);
    }
    
    // If we didn't fail, try to malloc the output string 
    if (!fail)
        out=(char*)OVR_ALLOC(len);
    // If that fails, we fail. 
    if (!out)
        fail = true;

    // Handle failure.
    if (fail)
    {
        for (int i=0; i<numentries; i++) 
        {
            if (entries[i])
                OVR_FREE(entries[i]);
        }
        OVR_FREE(entries);
        return 0;
    }
    
    // Compose the output array.
    *out='[';
    ptr=out+1;
    *ptr='\0';
    for (int i=0; i<numentries; i++)
    {
        OVR_strcpy(ptr, len - (ptr-out), entries[i]);
        ptr+=OVR_strlen(entries[i]);
        if (i!=numentries-1)
        {
            *ptr++=',';
            if (fmt)
                *ptr++=' ';
            *ptr='\0';
        }
        OVR_FREE(entries[i]);
    }
    OVR_FREE(entries);
    *ptr++=']';
    *ptr++='\0';
    return out;
}

//-----------------------------------------------------------------------------
// Build an object from the supplied text and returns the text position after
// the parsed object
const char* JSON::parseObject(const char* buff, const char** perror)
{
    if (*buff!='{')
    {
        return AssignError(perror, "Syntax Error: Missing opening brace");
    }
    
    Type=JSON_Object;
    buff=skip(buff+1);
    if (*buff=='}')
        return buff+1;    // empty array.
    
    JSON* child = new JSON();
    Children.PushBack(child);

    buff=skip(child->parseString(skip(buff), perror));
    if (!buff) 
        return 0;
    child->Name = child->Value;
    child->Value.Clear();
    
    if (*buff!=':')
    {
        return AssignError(perror, "Syntax Error: Missing colon");
    }

    buff=skip(child->parseValue(skip(buff+1), perror));    // skip any spacing, get the value.
    if (!buff)
        return 0;
    
    while (*buff==',')
    {
        child = new JSON();
        if (!child)
            return 0; // memory fail
        
        Children.PushBack(child);

        buff=skip(child->parseString(skip(buff+1), perror));
        if (!buff)
            return 0;
        
        child->Name=child->Value;
        child->Value.Clear();
        
        if (*buff!=':')
        {
            return AssignError(perror, "Syntax Error: Missing colon");
        }    // fail!
        
        // Skip any spacing, get the value.
        buff=skip(child->parseValue(skip(buff+1), perror));
        if (!buff)
            return 0;
    }
    
    if (*buff=='}')
        return buff+1;    // end of array 
    
    return AssignError(perror, "Syntax Error: Missing closing brace");
}

//-----------------------------------------------------------------------------
// Render an object to text.  The returned string must be freed
char* JSON::PrintObject(int depth, bool fmt)
{
    char**   entries = 0, **names = 0;
    char*    out = 0;
    char*    ptr, *ret, *str;
    intptr_t len = 7, i = 0, j;
    bool     fail = false;
    
    // Count the number of entries.
    int numentries = GetItemCount();
    
    // Explicitly handle empty object case
    if (numentries == 0)
    {
        out=(char*)OVR_ALLOC(fmt?depth+4:4);
        if (!out)
            return 0;
        ptr=out;
        *ptr++='{';
        
        if (fmt)
        {
            *ptr++='\n';
            for (i=0;i<depth-1;i++)
                *ptr++='\t';
        }
        *ptr++='}';
        *ptr++='\0';
        return out;
    }
    // Allocate space for the names and the objects
    entries=(char**)OVR_ALLOC(numentries*sizeof(char*));
    if (!entries)
        return 0;
    names=(char**)OVR_ALLOC(numentries*sizeof(char*));
    
    if (!names)
    {
        OVR_FREE(entries);
        return 0;
    }
    memset(entries,0,sizeof(char*)*numentries);
    memset(names,0,sizeof(char*)*numentries);

    // Collect all the results into our arrays:
    depth++;
    if (fmt)
        len+=depth;

    JSON* child = Children.GetFirst();
    while (!Children.IsNull(child))
    {
        names[i]     = str = PrintString(child->Name.ToCStr());
        entries[i++] = ret = child->PrintValue(depth, fmt);

        if (str && ret)
        {
            len += OVR_strlen(ret)+OVR_strlen(str)+2+(fmt?2+depth:0);
        }
        else
        {
            fail = true;
            break;
        }
        
        child = Children.GetNext(child);
    }
    
    // Try to allocate the output string
    if (!fail)
        out=(char*)OVR_ALLOC(len);
    if (!out)
        fail=true;

    // Handle failure
    if (fail)
    {
        for (i=0;i<numentries;i++)
        {
            if (names[i])
                OVR_FREE(names[i]);
            
            if (entries[i])
                OVR_FREE(entries[i]);}
        
        OVR_FREE(names);
        OVR_FREE(entries);
        return 0;
    }
    
    // Compose the output:
    *out = '{';
    ptr  = out+1;
    if (fmt)
    {
        *ptr++ = '\n';
    }
    *ptr = 0;
    
    for (i=0; i<numentries; i++)
    {
        if (fmt)
        {
            for (j = 0; j < depth; j++)
            {
                *ptr++ = '\t';
            }
        }
        OVR_strcpy(ptr, len - (ptr-out), names[i]);
        ptr   += OVR_strlen(names[i]);
        *ptr++ =':';
        
        if (fmt)
        {
            *ptr++ = '\t';
        }
        
        OVR_strcpy(ptr, len - (ptr-out), entries[i]);
        ptr+=OVR_strlen(entries[i]);
        
        if (i != numentries - 1)
        {
            *ptr++ = ',';
        }
        
        if (fmt)
        {
            *ptr++ = '\n';
        }
        *ptr = 0;
        
        OVR_FREE(names[i]);
        OVR_FREE(entries[i]);
    }
    
    OVR_FREE(names);
    OVR_FREE(entries);
    
    if (fmt)
    {
        for (i = 0; i < depth - 1; i++)
        {
            *ptr++ = '\t';
        }
    }
    *ptr++='}';
    *ptr++='\0';
    
    return out; 
}



// Returns the number of child items in the object
// Counts the number of items in the object.
unsigned JSON::GetItemCount() const
{
    unsigned count = 0;
    for (const JSON* child = Children.GetFirst(); !Children.IsNull( child ); child = child->pNext )
    {
        count++;
    }
    return count;
}

JSON* JSON::GetItemByIndex(unsigned index)
{
    for ( JSON * child = Children.GetFirst(); !Children.IsNull( child ); child = child->pNext )
	{
		if ( index-- == 0 )
		{
			return child;
		}
	}
	return 0;
}

const JSON* JSON::GetItemByIndex(unsigned index) const
{
    for ( const JSON * child = Children.GetFirst(); !Children.IsNull( child ); child = child->pNext )
	{
		if ( index-- == 0 )
		{
			return child;
		}
	}
	return 0;
}

// Returns the child item with the given name or NULL if not found
JSON* JSON::GetItemByName(const char* name)
{
    for ( JSON * child = Children.GetFirst(); !Children.IsNull( child ); child = child->pNext )
	{
		if ( OVR_strcmp( child->Name.ToCStr(), name ) == 0 )
		{
			return child;
		}
	}
	return 0;
}

// Returns the child item with the given name or NULL if not found
const JSON* JSON::GetItemByName(const char* name) const
{
    for ( const JSON * child = Children.GetFirst(); !Children.IsNull( child ); child = child->pNext )
	{
		if ( OVR_strcmp( child->Name.ToCStr(), name ) == 0 )
		{
			return child;
		}
	}
	return 0;
}

//-----------------------------------------------------------------------------
// Adds a new item to the end of the child list
void JSON::AddItem(const char *string, JSON *item)
{
    if (!item)
        return;
 
    item->Name = string;
    Children.PushBack(item);
}

// Helper function to simplify creation of a typed object
JSON* JSON::createHelper(JSONItemType itemType, double dval, const char* strVal)
{
    JSON *item = new JSON(itemType);
    if (item)
    {
        item->dValue = dval;
        if (strVal)
            item->Value = strVal;
    }
    return item;
}

//-----------------------------------------------------------------------------

bool JSON::GetBoolValue() const
{
    OVR_ASSERT( ( Type == JSON_Number ) || ( Type == JSON_Bool ) );
    OVR_ASSERT( dValue == 0.0 || dValue == 1.0 ); // if this hits, value is out of range
    return ( dValue != 0.0 );
}

int32_t JSON::GetInt32Value() const
{
    OVR_ASSERT( Type == JSON_Number );
    OVR_ASSERT( dValue >= INT_MIN && dValue <= INT_MAX ); // if this hits, value is out of range
    return (int32_t)dValue;
}

int64_t JSON::GetInt64Value() const
{
    OVR_ASSERT( Type == JSON_Number );
    OVR_ASSERT( dValue >= -9007199254740992LL && dValue <= 9007199254740992LL ); // 2^53 - if this hits, value is out of range
    return (int64_t)dValue;
}

float JSON::GetFloatValue() const
{
    OVR_ASSERT( Type == JSON_Number );
    OVR_ASSERT( dValue >= -FLT_MAX && dValue <= FLT_MAX );  // too large to represent as a float
    OVR_ASSERT( dValue == 0 || dValue <= -Mathf::MinPositiveValue || dValue >= Mathf::MinPositiveValue );  // if the number is too small to be represented as a float
    return (float)dValue;
}

double JSON::GetDoubleValue() const
{
    OVR_ASSERT( Type == JSON_Number );
    return dValue;
}

const String & JSON::GetStringValue() const
{
    OVR_ASSERT( Type == JSON_String );
    return Value;
}


//-----------------------------------------------------------------------------
// Adds an element to an array object type
void JSON::AddArrayElement(JSON *item)
{
    if (!item)
    {
        return;
    }

    Children.PushBack(item);
}

// Returns the size of an array
int JSON::GetArraySize()
{
    if (Type == JSON_Array)
    {
        return GetItemCount();
    }
    else
        return 0;
}

// Returns the number value an the give array index
double JSON::GetArrayNumber(int index)
{
    if (Type == JSON_Array)
    {
        JSON* number = GetItemByIndex(index);
        return number ? number->dValue : 0.0;
    }
    else
    {
        return 0;
    }
}

// Returns the string value at the given array index
const char* JSON::GetArrayString(int index)
{
    if (Type == JSON_Array)
    {
        JSON* number = GetItemByIndex(index);
        return number ? number->Value.ToCStr() : 0;
    }
    else
    {
        return 0;
    }
}

//-----------------------------------------------------------------------------
// Loads and parses the given JSON file pathname and returns a JSON object tree.
// The returned object must be Released after use.
JSON* JSON::Load(const char* path, const char** perror)
{
    SysFile f;
    if (!f.Open(path, File::Open_Read, File::Mode_Read))
    {
        AssignError(perror, "Failed to open file");
        return NULL;
    }

    int    len   = f.GetLength();
    uint8_t* buff  = (uint8_t*)OVR_ALLOC(len + 1);
    int    bytes = f.Read(buff, len);
    f.Close();

    if (bytes == 0 || bytes != len)
    {
        OVR_FREE(buff);
        return NULL;
    }

    // Ensure the result is null-terminated since Parse() expects null-terminated input.
    buff[len] = '\0';

    JSON* json = JSON::Parse((char*)buff, perror);
    OVR_FREE(buff);
    return json;
}

//-----------------------------------------------------------------------------
// Serializes the JSON object and writes to the give file path
bool JSON::Save(const char* path)
{
    SysFile f;
    if (!f.Open(path, File::Open_Write | File::Open_Create | File::Open_Truncate, File::Mode_Write))
        return false;

    char* text = PrintValue(0, true);
    if (text)
    {
        intptr_t len   = OVR_strlen(text);
        OVR_ASSERT(len <= (intptr_t)(int)len);

        int   bytes = f.Write((uint8_t*)text, (int)len);
        f.Close();
        OVR_FREE(text);
        return (bytes == len);
    }
    else
    {
        return false;
    }
}

//-----------------------------------------------------------------------------
// ***** JsonReader

const JSON * JsonReader::GetChildByName( const char * childName ) const
{
	assert( IsObject() );

	// Check if the the cached child pointer is valid.
	if ( !Parent->Children.IsNull( Child ) )
	{
		if ( OVR_strcmp( Child->Name.ToCStr(), childName ) == 0 )
		{
			const JSON * c = Child;
			Child = c->pNext;	// Cache the next child.
			return c;
		}
	}
	// Itereate over all children.
    for ( const JSON * c = Parent->Children.GetFirst(); !Parent->Children.IsNull( c ); c = c->pNext )
	{
		if ( OVR_strcmp( c->Name.ToCStr(), childName ) == 0 )
		{
			Child = c->pNext;	// Cache the next child.
			return c;
		}
	}
	return 0;
}

bool JsonReader::GetChildBoolByName( const char * childName, const bool defaultValue ) const
{
	const JSON * c = GetChildByName( childName );
	return ( c != NULL ) ? c->GetBoolValue() : defaultValue;
}

int32_t JsonReader::GetChildInt32ByName( const char * childName, const int32_t defaultValue ) const
{
	const JSON * c = GetChildByName( childName );
	return ( c != NULL ) ? c->GetInt32Value() : defaultValue;
}

int64_t JsonReader::GetChildInt64ByName( const char * childName, const int64_t defaultValue ) const
{
	const JSON * c = GetChildByName( childName );
	return ( c != NULL ) ? c->GetInt64Value() : defaultValue;
}

float JsonReader::GetChildFloatByName( const char * childName, const float defaultValue ) const
{
	const JSON * c = GetChildByName( childName );
	return ( c != NULL ) ? c->GetFloatValue() : defaultValue;
}

double JsonReader::GetChildDoubleByName( const char * childName, const double defaultValue ) const
{
	const JSON * c = GetChildByName( childName );
	return ( c != NULL ) ? c->GetDoubleValue() : defaultValue;
}

const String JsonReader::GetChildStringByName( const char * childName, const String & defaultValue ) const
{
	const JSON * c = GetChildByName( childName );
	return String( ( c != NULL ) ? c->GetStringValue() : defaultValue );
}

const JSON * JsonReader::GetNextArrayElement() const
{
	assert( IsArray() );

	// Check if the the cached child pointer is valid.
	if ( !Parent->Children.IsNull( Child ) )
	{
		const JSON * c = Child;
		Child = c->pNext;	// Cache the next child.
		return c;
	}
	return NULL;
}

bool JsonReader::GetNextArrayBool( const bool defaultValue ) const
{
	const JSON * c = GetNextArrayElement();
	return ( c != NULL ) ? c->GetBoolValue() : defaultValue;
}

int32_t JsonReader::GetNextArrayInt32( const int32_t defaultValue ) const
{
	const JSON * c = GetNextArrayElement();
	return ( c != NULL ) ? c->GetInt32Value() : defaultValue;
}

int64_t JsonReader::GetNextArrayInt64( const int64_t defaultValue ) const
{
	const JSON * c = GetNextArrayElement();
	return ( c != NULL ) ? c->GetInt64Value() : defaultValue;
}

float JsonReader::GetNextArrayFloat( const float defaultValue ) const
{
	const JSON * c = GetNextArrayElement();
	return ( c != NULL ) ? c->GetFloatValue() : defaultValue;
}

double JsonReader::GetNextArrayDouble( const double defaultValue ) const
{
	const JSON * c = GetNextArrayElement();
	return ( c != NULL ) ? c->GetDoubleValue() : defaultValue;
}

const String JsonReader::GetNextArrayString( const String & defaultValue ) const
{
	const JSON * c = GetNextArrayElement();
	return String( ( c != NULL ) ? c->GetStringValue() : defaultValue );
}

} // namespace OVR
