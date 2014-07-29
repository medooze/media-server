#include "cpim.h"
#include <string.h>


MIMEText* MIMEText::Parse(const BYTE* buffer,DWORD size)
{
        UTF8Parser utf8;
        //Parrse
        if (!utf8.Parse(buffer,size))
                return NULL;
        //Create new
        return new MIMEText(utf8.GetWString());
}

DWORD MIMEText::Serialize(BYTE* buffer,DWORD size)
{
        //Convert to utf8
        UTF8Parser utf8(*this);
        //Serialize
        return utf8.Serialize(buffer,size);
}

DWORD MIMEWrapper::Serialize(BYTE* buffer,DWORD size)
{
        //Check nulls
        if (!contentType || !object)
                return 0;

        DWORD pos = 0;

        //Serialize content type
        DWORD len = contentType->Serialize((char*)buffer+pos,size-pos);
        //Check
        if (!len) return 0;
        //move
        pos += len;

        //Check size for \r\n
        if (pos+2>size) return 0;
        //add extra line
        buffer[pos++] = '\r';
        buffer[pos++] = '\n';

        //Serialize object
        len = object->Serialize(buffer+pos,size-pos);

        //Check
        if (!len) return 0;

        //Move
        pos+=len;

        //Ok
        return pos;
}

MIMEWrapper* MIMEWrapper::Parse(const BYTE* data,DWORD size)
{
       MIMEWrapper* wrapper = new MIMEWrapper();
       UTF8Parser utf8;
       DWORD pos = 0;
       std::string header;
       
       //Get first line
       StringParser parser((char*)data,size);

       while(!parser.CheckString("\r\n") && parser.Left()>2)
       {
            //Parse line
            if (!parser.ParseToken())
                    //error
                    goto error;
            
            //Get value
            header = parser.GetValue();
            
            //Match sparator
            if (!parser.MatchString(": "))
                    //error
                    goto error;
            
            //Check header case insensitive
            if (strcasecmp("content-type",header.c_str())==0)
            {
                //Get until end of line
                if (!parser.ParseLine())
                     //error
                       goto error;
                //Parse content type
                wrapper->contentType = ContentType::Parse(parser.GetValue());
           } else {
                //Get until end of line
                if (!parser.ParseLine())
                     //error
                       goto error;
           }
       }
       
        //If not found
        if (!wrapper->contentType)
                //error
                goto error;
       
        //Parse CRLF
       if (!parser.MatchString("\r\n"))
                //error
                goto error;

        //Get current position
        pos = parser.GetPos();

       //Check content type
       if (wrapper->contentType->Equals("text","plain"))
       {
               //Create Text object
               wrapper->object =  MIMEText::Parse(data+pos,size-pos);
               //Check it
               if (!wrapper->object)
                       //error
                       goto error;
       } else {
               //Not handled yet
               //error
               goto error;
       }

       //return object
       return wrapper;

error:
       //Clean
       delete wrapper;
       //Error
       return NULL;
}

DWORD Address::Serialize(BYTE* buffer,DWORD size) const
{
        DWORD pos = 0;

        //If we got a display name
        if (!displayName.empty())
        {
                //In utf8
                UTF8Parser utf8(displayName);
                //Serialize display name
                DWORD len = utf8.Serialize(buffer+pos,size-pos);
                //Check it has been correcrt
                if (!len)
                        return 0;
                //Move
                pos+=len;
                //check size for extra espace
                if (pos+1>size)
                        return 0;
                //Add SP
                buffer[pos++] = ' ';
        }

        //check size for uri start
        if (pos+1>size)
                return 0;
        //Add <
        buffer[pos++] = '<';

        //Now add utf8 uri
        UTF8Parser utf8(uri);
        //Serialize it
        DWORD len = utf8.Serialize(buffer+pos,size-pos);
        //Check it has been 
        if (!len)
                return 0;
        //Move
        pos+=len;

        //check size for uri end
        if (pos+1>size)
                return 0;
        //Add <
        buffer[pos++] = '>';

        return pos;
}

CPIMMessage* CPIMMessage::Parse(const BYTE* data,DWORD size)
{
    DWORD pos = 0;
    Address from;
    Address to;

    //Parse headers until empty line
    while(pos+1<size && (data[pos]!=13 || data[pos+1]!=10))
    {
        //Parse header name
        StringParser hp((char*)data+pos,size-pos);
        //Get header
        if (!hp.ParseToken())
            //Error
            return 0;
        //Get value
        std::string header = hp.GetValue();
        //Find :
        if (!hp.MatchString(": "))
            //Error
            return 0;

        //Move
        pos += hp.GetPos();

        //get start of value
        DWORD start = pos;

        //Check two chars still on buffer
        if (pos+1>size)
            //Error
            return 0;

        //Find CRLF
        while ((data[pos]!=13 || data[pos+1]!=10))
        {
            //increase
            pos++;
            //Check two chars still on buffer
            if (pos+1>size)
                //Error
                return 0;
        }
        //Get end of header
        DWORD end = pos;
        //Skip \r\n
        pos+=2;

        //Check header
        if (header.compare("From")==0) 
        {
            std::wstring display;
            std::wstring uri;
            //Parse UTF8
            UTF8Parser parser;
            //Parse
            if (!parser.Parse(data+start,end-start))
                    return 0;

            //String parser
            WideStringParser wp(parser.GetWString());

            //Parse 
            // Formal-name  = 1*( Token SP ) / String
            if (wp.ParseQuotedString())
            {
                //get it
                display = wp.GetValue();
                //Skip any space
                wp.SkipSpaces();
            } else {
                //Check end
                while(!wp.CheckChar('<'))
                {
                    //Parse token
                    if (!wp.ParseToken())
                        return 0;
                    //Check if its empty to append SP
                    if (!display.empty())
                        //add it
                        display += L" ";
                    //Add parsed token
                    display += wp.GetValue();
                    //Skip any space
                    wp.SkipSpaces();
                }
            }

            //Parse <
            if (!wp.ParseChar('<'))
                return 0;

            //UNtil >
            if (!wp.ParseUntilCharset(L">"))
                return 0;

            //Get uri
           uri = wp.GetValue();

            //Set them
            from.SetAddress(display,uri);

        } else if (header.compare("To")==0) {
            std::wstring display;

            //Parse UTF8
            UTF8Parser parser;
            //Parse
            if (!parser.Parse(data+start,end-start))
                    return 0;

            //String parser
            WideStringParser wp(parser.GetWString());

            //Parse 
            // Formal-name  = 1*( Token SP ) / String
            if (wp.ParseQuotedString())
            {
                //get it
                display = wp.GetValue();
                //Skip any space
                wp.SkipSpaces();
            } else {
                //Check end
                while(!wp.CheckChar('<'))
                {
                    //Parse token
                    if (!wp.ParseToken())
                        return 0;
                    //Check if its empty to append SP
                    if (!display.empty())
                        //add it
                        display += L" ";
                    //Add parsed token
                    display += wp.GetValue();
                    //Skip any space
                    wp.SkipSpaces();
                }
            }

            //Parse <
            if (!wp.ParseChar('<'))
                return 0;

            //UNtil >
            if (!wp.ParseUntilCharset(L">"))
                return 0;

            //Get uri
            std::wstring uri = wp.GetValue();
            //Set them
            to.SetAddress(display,uri);
        } else {
            //Ignore the rest fornow
        }
    }
    //Check two chars still on buffer
    if (pos+1>size)
        //Error
        return 0;
    
    //Skip \r\n
    pos+=2;

    //Now parse mime
     MIMEWrapper *mime = MIMEWrapper::Parse(data+pos,size-pos);

     //If not parsed
     if (!mime)
         return 0;

    //Create cpim
    return new CPIMMessage(from,to,mime);
}

DWORD CPIMMessage::Serialize(BYTE* buffer,DWORD size) const
{
    DWORD pos = 0;
    DWORD len = 0;

    //Write from header
    if (pos+6>size) return 0;
    memcpy(buffer+pos,"From: ",6);
    pos+=6;
    //serialize value
    len = from.Serialize(buffer+pos,size-pos);
    if (!len) return 0;
    pos+=len;
    //Add end line
    if (pos+2>size)  return 0;
    memcpy(buffer+pos,"\r\n",2);
    pos+=2;

    //Write to header
    if (pos+6>size) return 0;
    memcpy(buffer+pos,"From: ",6);
    pos+=6;
    //serialize value
    len = from.Serialize(buffer+pos,size-pos);
    if (!len)  return 0;
    pos+=len;
     //Add end line
    if (pos+2>size) return 0;
    memcpy(buffer+pos,"\r\n",2);
    pos+=2;

    //Add emtpy line
     if (pos+2>size) return 0;
     memcpy(buffer+pos,"\r\n",2);
     pos+=2;

     //Now write mime wrapper
     len = mime->Serialize(buffer+pos,size-pos);
     if (!len) return 0;
     pos+=len;

     //OK
     return pos;
}