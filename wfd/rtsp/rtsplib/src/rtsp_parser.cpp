/***************************************************************************
 *                             rtsp_parser.cpp
 * DESCRIPTION
 *  RTSP message parser definition for RTSP_LIB module
 *
 * Copyright (c) 2011-2016 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 ***************************************************************************/

#include "rtsp_session.h"
#include "rtsp_wfd.h"
#include "rtsp_parser.h"


extern unsigned stringToNum(string value);
/*
 * Erase up to first occurrence of character 'c' in string "input"
 */
extern string findStart(char *input, char c);

static bool isDelimiter(char c, const char *delim, size_t delimBufsize);

static void computeStringLengthUptoDelimiter(
   size_t& retSize,
   const char *source, size_t sourceLen,
   const char *delim, size_t delimBufsize);


void createWordTable(string input, char **wordTable, unsigned &wordTableLen)
{
    wordTableLen = 0;
    char *pTempPtr = NULL;

    if (char *str = strtok_r((char *)input.c_str(), " ", &pTempPtr)) {
        do {
            size_t len = strlen(str) + 1;
            wordTable[wordTableLen] = (char *)MM_Malloc(len *sizeof (char));
            if(wordTable[wordTableLen] == NULL)
            {
               MM_MSG_PRIO(MM_GENERAL, MM_PRIO_ERROR,"RTSP_LIB :: wordTable memory allocation failed");
               return;
            }
            strlcpy(wordTable[wordTableLen], str, len);
            wordTableLen++;
        } while ((str = strtok_r(NULL, " ", &pTempPtr)));
    }
}

bool getRtspParam(char **wordTable, unsigned wordTableLen, rtspFields field, string &output,const char *input, bitset<WFD_MAX_SIZE> wfdMethods)
{
    UNUSED(wordTableLen);
    char *pTempPtr = NULL;
    if(wordTable[0] != NULL)
    {
        switch(field) {
        case getSequence:
            if (!strncasecmp(cSeq, wordTable[0], strlen(cSeq))) {
                if(wordTable[1] != NULL)
                {
                  output = wordTable[1];
                  return true;
                }
            }
        break;
        case getSession:
            if (!strncasecmp(session, wordTable[0], strlen(session))) {
                if(wordTable[1] != NULL)
                {
                  output = wordTable[1];
                  return true;
                }
            }
        break;
        case getRtpPorts:
            pTempPtr = NULL;
            if (!strncasecmp(transport, wordTable[0], strlen(transport))) {

                output = "";

                // Uncomment for RTCPtest strlcat ( wordTable[1],";client_port=19000-19001", 256);
                char *str = NULL;
                if(wordTable[1] != NULL)
                {
                  bool bOk = false;
                  str = wordTable[1];

                  MM_MSG_SPRINTF_PRIO_1(MM_GENERAL, MM_PRIO_HIGH,
                      "getRtpPorts parse '%s' to find clientPort", str);

                  char *tmp = strstr(str, clientPort);

                  if (tmp)
                  {
                      // found "client_port="
                      bOk = true;

                      size_t clientPortsSize = 0;
                      computeStringLengthUptoDelimiter(
                         clientPortsSize, tmp, strlen(tmp), "\r\n;", sizeof("\r\n;"));

                      if (clientPortsSize > 0)
                      {
                          output.append(tmp, clientPortsSize);
                          MM_MSG_SPRINTF_PRIO_2(MM_GENERAL, MM_PRIO_HIGH,
                              "getRtpPorts: parsed client_port '%s', len=%u",
                              output.c_str(), clientPortsSize);
                      }
                      else
                      {
                          MM_MSG_SPRINTF_PRIO_1(MM_GENERAL, MM_PRIO_ERROR,
                              "Failed to fetch value of client_port from '%s'", str);
                      }
                  }

                  if (true == bOk)
                  {
                      // look for server ports only if finding client port was successful.
                      char *tmp = strstr(str, serverPort);

                      if (tmp)
                      {
                          // found "server_port"
                          size_t serverPortsSize = 0;
                          computeStringLengthUptoDelimiter(
                             serverPortsSize, tmp, strlen(tmp), "\r\n;", sizeof("\r\n;"));

                          output.append(";");

                          if (serverPortsSize > 0)
                          {
                              output.append(tmp, serverPortsSize);
                              MM_MSG_SPRINTF_PRIO_2(MM_GENERAL, MM_PRIO_HIGH,
                                  "getRtpPorts: parsed server_port '%s', len=%u",
                                  output.c_str(), serverPortsSize);
                          }
                          else
                          {
                              MM_MSG_SPRINTF_PRIO_1(MM_GENERAL, MM_PRIO_ERROR,
                                                    "Failed to fetch value of server_port from '%s'", str);
                          }
                      }
                  }

                  MM_MSG_SPRINTF_PRIO_1(MM_GENERAL, MM_PRIO_HIGH,
                                        "getRtpPorts: output '%s'", output.c_str());
                  return bOk;
                }
            }
        break;
        case getWfdMethod:
            {
              bitset<WFD_MAX_SIZE> checkWFDMethods;
              char *pTempPtr = NULL;
              checkWFDMethods.reset();
              /* methods are case sensitive comparision */
              if (!strncmp(methods, wordTable[0], strlen(methods))) {

                for (int i = 1; i < numSupportedCmds; i++) {

                      if((pTempPtr = strstr((const char *)input, (const char *) supportedCmds[i].cmdName.c_str())) != NULL){
                          output = supportedCmds[i].cmdName.c_str();
                          checkWFDMethods.set(supportedCmds[i].cmd);
                        }
                }

                if( (wfdMethods & checkWFDMethods) == wfdMethods)
                {
                  MM_MSG_PRIO(MM_GENERAL, MM_PRIO_HIGH,"RTSP_LIB :: RTSP Mandatory methods present");
                }
                else
                {
                  MM_MSG_PRIO2(MM_GENERAL, MM_PRIO_ERROR,"RTSP_LIB :: Tearing Down "\
                    "Session : Mandatory RTSP Methods not present "\
                    "Required Methods Bitmap = %lu Received Bitmap = %lu",wfdMethods.to_ulong(),checkWFDMethods.to_ulong());
                  SET_ERROR;
                }
               return true;
              }
           }
        break;
        default:
        break;
        }
    }
    return false;
}

/*
 * Second level parser
 *
 * Parse through the array of native RTSP strings created by recvCmd below
 */
void parseRecv(char **input, size_t len, rtspParams *params)
{
    rtspSessionState *state = &params->state;
    char *str = input[0];
    rtspWfd *wfd = &params->state.wfd;
    char *wordTable[MAXLEN];
    unsigned wordTableLen = 0;
    bool set = false;
    char *pTempPtr = NULL;


    params->state.wfd.tcpWindowSize.setValid(false);
    params->state.wfd.client.setRenegotiated(false);


    MM_MSG_PRIO(MM_GENERAL, MM_PRIO_HIGH, "RTSP_LIB :: Entering parseRecv");



    if ((str = strtok_r(str, " ", &pTempPtr))) {
        for (int j = 1; j < numSupportedCmds; j++) {
            if (!strncmp(supportedCmds[j].cmdName.c_str(), str, strlen(supportedCmds[j].cmdName.c_str()))) {
                params->mesg.cmd = supportedCmds[j].cmd;
                params->mesg.cmdName = supportedCmds[j].cmdName;
                params->mesg.type = cmdRequest;
                params->valid |= mesgValid;
                MM_MSG_PRIO1(MM_GENERAL, MM_PRIO_HIGH, "RTSP_LIB :: Received %s" , supportedCmds[j].cmdName.c_str());
                break;
            }
        }

        if (!strcmp(rtspVersion, str)) {
            params->valid |= mesgValid;
            params->valid |= statusValid;
            params->status = error;
            if ((str = strtok_r(NULL, " ", &pTempPtr))) {
                params->respCode = str;
                if (!strcmp(successCode, str))  {
                    if ((str = strtok_r(NULL, " ", &pTempPtr))) {
                        if (!strcmp(success, str)) {
                            params->status = ok;
                        }
                    }
                }
            }
        }
    }

    for (unsigned i = 1; i < len; i++) {
        string line = string(input[i]);
        string copy = string(input[i]);
        rtspWfdParams type;
        string parsedRxSeq, parsedSession, parsedRtp, parsedWfdMethod;

        createWordTable(copy, wordTable, wordTableLen);
        if(wordTableLen == 0)
        {
            MM_MSG_PRIO(MM_GENERAL, MM_PRIO_ERROR, "RTSP_LIB :: wordTable was not created");
            continue;
        }
        if (getRtspParam(wordTable, wordTableLen, getSequence, parsedRxSeq)) {
            RTSPStringStream out;
            state->rxCseq = atoi(parsedRxSeq.c_str());
            params->valid |= cRxSeqValid;
            out << "Received CSeq: ";
            out << state->rxCseq;
            MM_MSG_PRIO1(MM_GENERAL, MM_PRIO_HIGH, "RTSP_LIB :: %s", out.str().c_str());
            for (unsigned j = 0; j < wordTableLen; j++) {
              if (wordTable[j])
              RTSP_FREEIF(wordTable[j]);
            }
            continue;
        } else if (getRtspParam(wordTable, wordTableLen, getSession,
                   parsedSession)) {
            RTSPStringStream out;
            state->sessStr = parsedSession;
            params->valid |= sessionValid;
            out << "Received Session: ";
            out << state->sessStr;
            MM_MSG_PRIO1(MM_GENERAL, MM_PRIO_HIGH, "RTSP_LIB :: %s", out.str().c_str());
            for (unsigned j = 0; j < wordTableLen; j++) {
               if ( wordTable[j])
                 RTSP_FREEIF(wordTable[j]);
            }
            continue;

        } else if (getRtspParam(wordTable, wordTableLen, getWfdMethod,
                   parsedWfdMethod,(const char *)copy.c_str(),params->mesg.wfdOptionsParams)) {
            state->wfdSupp = true;
            params->valid |= wfdValid;
            for (unsigned j = 0; j < wordTableLen; j++) {
               if ( wordTable[j])
                RTSP_FREEIF(wordTable[j]);
            }
            continue;
        } else if (getRtspParam(wordTable, wordTableLen, getRtpPorts,
                   parsedRtp)) {
            if(parsedRtp.length()) {
                MM_MSG_PRIO1(MM_GENERAL, MM_PRIO_HIGH,
                    "RTSP_LIB :: Fetch ports from %s", (char*)parsedRtp.c_str());

                {
                    const char *tmp = strstr(parsedRtp.c_str(), clientPort);

                    if (tmp)
                    {
                        tmp += strlen(clientPort);
                        char *hyphen = strstr(tmp, "-");

                        if(hyphen)
                        {
                            ++hyphen;
                            size_t clientRtcpPortStrSize = 0;
                            computeStringLengthUptoDelimiter(
                               clientRtcpPortStrSize, hyphen, strlen(hyphen),
                               "\r\n;", sizeof("\r\n;"));

    char tmpStr[20];
                          memset(tmpStr, 0, sizeof(tmpStr));
                          const unsigned port = (strlcpy(tmpStr, hyphen, sizeof(tmpStr)) >= sizeof(tmpStr)) ? 0 : stringToNum(tmpStr);

                            MM_MSG_PRIO1(MM_GENERAL, MM_PRIO_HIGH,
                                "RTSP_LIB :: RTCP client_port %u", (unsigned int)port);

                          // set rtcp port so parsed response msg in rtsp_base can fetch this
                          wfd->client.setRtcpPort0(port);
                      }
                  }
                  else
                  {
                      MM_MSG_PRIO(MM_GENERAL, MM_PRIO_HIGH,
                                  "rtcpcctrl: did not find client_port");
                  }
                }

                {
                    // get server port
                    const char *tmp = strstr(parsedRtp.c_str(), serverPort);
                    unsigned int prevPort = wfd->server.getRtcpPort0();

                    if (tmp)
                    {
                        tmp += strlen(serverPort);
                        char *hyphen = strstr(tmp, "-");

                        if(hyphen)
                        {
                            ++hyphen;
                            size_t rtcpPortStrSize = 0;
                            computeStringLengthUptoDelimiter(rtcpPortStrSize, hyphen, strlen(hyphen), "\r\n;", sizeof("\r\n;"));
                            char tmpStr[20];
                            memset(tmpStr, 0, sizeof(tmpStr));
                            const unsigned port = (strlcpy(tmpStr, hyphen, sizeof(tmpStr)) >= sizeof(tmpStr)) ? 0 : stringToNum(tmpStr);

                            MM_MSG_PRIO1(MM_GENERAL, MM_PRIO_HIGH,
                                "RTSP_LIB :: RTCP server_port %u", (unsigned int)port);

                            // set rtcp port so parsed response msg in rtsp_base can fetch this
                            wfd->server.setRtcpPort0(port);
                        }
                    }
                    else
                    {
                        MM_MSG_PRIO(MM_GENERAL, MM_PRIO_HIGH,
                                    "rtcpcctrl: did not find server_port");
                    }
                }
            }

            for (unsigned j = 0; j < wordTableLen; j++) {
               if ( wordTable[j])
                 RTSP_FREEIF(wordTable[j]);
            }
            continue;
        }

        for (unsigned j = 0; j < wordTableLen; j++) {
           if ( wordTable[j])
             RTSP_FREEIF(wordTable[j]);
        }

        if ((type = wfd->wfdType(line, set)) != wfd_invalid) {
            if (true == set) {
                wfd->wfdParse(type, line);
            }
            params->mesg.wfdParams.set(type);
            if (type == wfd_trigger_method) {
                RTSPStringStream ss;
                ss << wfd->trigger;
                string tmp = ss.str();

                for (int k = 1; k < numSupportedCmds; k++) {
                    if (!strcmp(supportedCmds[k].cmdName.c_str(), tmp.c_str()))
                    {
                        params->mesg.subCmd = supportedCmds[k].cmd;
                    }
                }
            }
        }
    }

    MM_MSG_PRIO(MM_GENERAL, MM_PRIO_HIGH, "RTSP_LIB :: Exiting parseRecv");
}

/*
 * First level parser
 *
 * Creates an array of lines that have been delimited by new-lines.
 * This is followed by an array of strings that have been delimited by spaces
 */
void recvCmd(string input, rtspParams *params)
{
    static char *lineTable[MAXLEN];
    static char *parseTable[MAXLEN];
    unsigned lineTableLen = 0;
    char *pTempPtr = NULL;
    char *str = strtok_r((char *)input.c_str(), crlf, &pTempPtr);
    list<unsigned> delims;
    rtspParams *tmp = params;

    MM_MSG_PRIO(MM_GENERAL, MM_PRIO_HIGH, "RTSP_LIB :: Entering recvCmd");

    memset(lineTable, 0, sizeof(lineTable));
    memset(parseTable,0,sizeof(parseTable));

    if (str) {
        size_t len = strlen(str)+1;
        lineTable[0] = (char*)MM_Malloc(len*sizeof(char));
        if(lineTable[0] == NULL)
        {
            MM_MSG_PRIO(MM_GENERAL, MM_PRIO_ERROR,"RTSP_LIB :: lineTable memory allocation failed");
            return;
        }
        strlcpy(lineTable[0], str, len);
    } else
        return;

    for (lineTableLen = 1; lineTableLen < MAXLEN; lineTableLen++) {
        if ((str = strtok_r(NULL, crlf, &pTempPtr))) {
            size_t len = strlen(str) + 1;
            lineTable[lineTableLen] = (char*)MM_Malloc(len*sizeof(char));
            if(lineTable[lineTableLen] == NULL)
            {
               MM_MSG_PRIO(MM_GENERAL, MM_PRIO_ERROR,"RTSP_LIB :: lineTable memory allocation failed");
               return;
            }
            strlcpy(lineTable[lineTableLen], str, len);
            parseTable[lineTableLen] = (char*)MM_Malloc(len*sizeof(char));
            if(parseTable[lineTableLen]== NULL)
            {
               MM_MSG_PRIO(MM_GENERAL, MM_PRIO_ERROR,"RTSP_LIB :: parseTable memory allocation failed");
               return;
            }
            strlcpy(parseTable[lineTableLen], str, len);
        } else
            break;
    }

    pTempPtr = NULL;
    delims.insert(delims.begin(), 0);
    for (unsigned j = 1; j < lineTableLen; j++) {
        char *str = parseTable[j];
        if ((str = strtok_r(str, " ", &pTempPtr))) {
            for (int i = 1; i < numSupportedCmds; i++) {
                if ((!strcmp(supportedCmds[i].cmdName.c_str(), str)))
                    delims.insert(delims.end(), j);
            }
            if (!strcmp(rtspVersion, str)) {
                delims.insert(delims.end(), j);
            }
        }
    }
    delims.insert(delims.end(), lineTableLen);

    for (unsigned i = 0; i < (delims.size() - 2); i++) {
        tmp->next = MM_New(rtspParams);
        if(tmp->next == NULL)
        {
            MM_MSG_PRIO(MM_GENERAL, MM_PRIO_ERROR,"RTSP_LIB ::  memory allocation failed");
            return;
        }
        tmp = tmp->next;
        tmp->reset();
    }

    while (delims.size() > 1) {
        size_t begin = delims.front();
        delims.pop_front();
        size_t end = delims.front();
        char **start = &lineTable[begin];
        parseRecv(start, end-begin, params);
        params = params->next;
    }

    for (unsigned i = 0; i < lineTableLen; i++) {
        if (lineTable[i])
          RTSP_FREEIF(lineTable[i]);
        if (parseTable[i])
          RTSP_FREEIF(parseTable[i]);
    }

    MM_MSG_PRIO(MM_GENERAL, MM_PRIO_HIGH, "RTSP_LIB :: Exiting recvCmd");
}

/**
 * Check if input 'c' is a delimiter
 */
bool isDelimiter(char c, const char *delim, size_t delimBufsize)
{
    bool retVal = false;

    for (size_t i = 0; i < delimBufsize; ++i)
    {
        if (c == delim[i] || '\0' == c)
        {
            retVal = true;
            break;
        }
    }

    return retVal;
}

/**
 * Compute the length of string from input 'source' upto a
 * specified delimiter. If delimiter is not found, then the
 * computed string length evaluates to the input 'sourceLen'
 */
void computeStringLengthUptoDelimiter(
   size_t& retSize,
   const char *source, size_t sourceLen,
   const char *delim, size_t delimBufsize)
{
    retSize = 0;

    if (source && sourceLen > 0 &&
        delim && delimBufsize > 0)
    {
        bool bFoundDelim = false;

        for (size_t i = 0; i < sourceLen; ++i)
        {
            if (true == isDelimiter(source[i], delim, delimBufsize))
            {
                retSize = i; // pointing at delimiter
                bFoundDelim = true;
                break;
            }
        }

        if (false == bFoundDelim)
        {
            MM_MSG_PRIO1(MM_GENERAL, MM_PRIO_HIGH,
                         "computeStringLengthUptoDelimiter did not find delimiter, "
                         "set computed size=%u", sourceLen);
            retSize = sourceLen;
        }
    }

    MM_MSG_SPRINTF_PRIO_2(MM_GENERAL, MM_PRIO_HIGH,
        "computeStringLengthUptoDelimiter for '%s'=%u", source, retSize);
}
