/*
 * Copyright (c) 2015 Ambroz Bizjak
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef APRINTER_WEB_INTERFACE_MODULE_H
#define APRINTER_WEB_INTERFACE_MODULE_H

#include <stddef.h>
#include <string.h>

#include <aprinter/meta/WrapFunction.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/AliasStruct.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/ProgramMemory.h>
#include <aprinter/base/Assert.h>
#include <aprinter/net/http/HttpServer.h>
#include <aprinter/fs/BufferedFile.h>
#include <aprinter/fs/DirLister.h>
#include <aprinter/misc/StringTools.h>
#include <aprinter/printer/utils/JsonBuilder.h>

#define APRINTER_ENABLE_HTTP_TEST 1

#include <aprinter/BeginNamespace.h>

template <typename Context, typename ParentObject, typename ThePrinterMain, typename Params>
class WebInterfaceModule {
public:
    struct Object;
    
private:
    using TheHttpServerService = HttpServerService<
        typename Params::HttpServerNetParams,
        128,   // MaxRequestLineLength
        64,    // MaxHeaderLineLength
        250,   // ExpectedResponseLength
        10000, // MaxRequestHeadLength
        256,   // MaxChunkHeaderLength
        1024,  // MaxTrailerLength
        4,     // TxChunkHeaderDigits
        4      // MaxQueryParams
    >;
    
    struct HttpRequestHandler;
    struct UserClientState;
    using TheHttpServer = typename TheHttpServerService::template Server<Context, Object, ThePrinterMain, HttpRequestHandler, UserClientState>;
    using TheRequestInterface = typename TheHttpServer::TheRequestInterface;
    
    using TheFsAccess = typename ThePrinterMain::template GetFsAccess<>;
    using TheBufferedFile = BufferedFile<Context, TheFsAccess>;
    using TheDirLister = DirLister<Context, TheFsAccess>;
    using FsEntry   = typename TheFsAccess::TheFileSystem::FsEntry;
    using EntryType = typename TheFsAccess::TheFileSystem::EntryType;
    
    static size_t const JsonBufferSize = Params::JsonBufferSize;
    static_assert(JsonBufferSize <= TheHttpServer::MaxGuaranteedBufferAvailBeforeHeadSent, "");
    static_assert(JsonBufferSize >= 128, "");
    static_assert(JsonBufferSize >= TheFsAccess::TheFileSystem::MaxFileNameSize + 6, "");
    
    static size_t const GetSdChunkSize = 512;
    static_assert(GetSdChunkSize <= TheHttpServer::MaxTxChunkSize, "");
    
    static constexpr char const * WebRootPath() { return "www"; }
    static constexpr char const * IndexPage() { return "reprap.htm"; }
    static constexpr char const * UploadBasePath() { return nullptr; }
    
public:
    static void init (Context c)
    {
        TheHttpServer::init(c);
    }
    
    static void deinit (Context c)
    {
        TheHttpServer::deinit(c);
    }
    
private:
    static char const * get_content_type (MemRef path)
    {
        if (AsciiCaseInsensEndsWith(path, ".htm") || AsciiCaseInsensEndsWith(path, ".html")) {
            return "text/html";
        }
        if (AsciiCaseInsensEndsWith(path, ".css")) {
            return "text/css";
        }
        if (AsciiCaseInsensEndsWith(path, ".js")) {
            return "application/javascript";
        }
        if (AsciiCaseInsensEndsWith(path, ".png")) {
            return "image/png";
        }
        if (AsciiCaseInsensEndsWith(path, ".ico")) {
            return "image/x-icon";
        }
        return "application/octet-stream";
    }
    
    static void http_request_handler (Context c, TheRequestInterface *request)
    {
        char const *method = request->getMethod(c);
        MemRef path = request->getPath(c);
        UserClientState *state = request->getUserState(c);
        
        if (!strcmp(method, "GET")) {
            if (request->hasRequestBody(c)) {
                goto bad_request;
            }
            
#if APRINTER_ENABLE_HTTP_TEST
            if (path.equalTo("/downloadTest")) {
                return state->acceptDownloadTestRequest(c, request);
            }
#endif
            
            if (path.equalTo("/rr_files")) {
                MemRef dir_path;
                if (!request->getParam(c, "dir", &dir_path)) {
                    goto bad_params;
                }
                
                bool flag_dirs = false;
                MemRef flag_dirs_param;
                if (request->getParam(c, "flagDirs", &flag_dirs_param)) {
                    flag_dirs = flag_dirs_param.equalTo("1");
                }
                
                return state->acceptFilesRequest(c, request, dir_path.ptr, flag_dirs);
            }
            
            if (path.removePrefix("/rr_")) {
                return state->acceptJsonResponseRequest(c, request, path);
            }
            
            if (path.ptr[0] == '/') {
                char const *file_path = path.equalTo("/") ? IndexPage() : (path.ptr + 1);
                return state->acceptGetFileRequest(c, request, file_path);
            }
        }
        else if (!strcmp(method, "POST")) {
            if (!request->hasRequestBody(c)) {
                goto bad_request;
            }
            
#if APRINTER_ENABLE_HTTP_TEST
            if (path.equalTo("/uploadTest")) {
                return state->acceptUploadTestRequest(c, request);
            }
#endif
            
            if (path.equalTo("/rr_upload")) {
                MemRef file_name;
                if (!request->getParam(c, "name", &file_name)) {
                    goto bad_params;
                }
                
                return state->acceptUploadFileRequest(c, request, file_name.ptr);
            }
        }
        else {
            request->setResponseStatus(c, HttpStatusCodes::MethodNotAllowed());
            goto error;
        }
        
        request->setResponseStatus(c, HttpStatusCodes::NotFound());
        
    error:
        if (false) bad_request: {
            request->setResponseStatus(c, HttpStatusCodes::BadRequest());
        }
        if (false) bad_params: {
            request->setResponseStatus(c, HttpStatusCodes::UnprocessableEntity());
        }
        request->completeHandling(c);
    }
    struct HttpRequestHandler : public AMBRO_WFUNC_TD(&WebInterfaceModule::http_request_handler) {};
    
    static bool handle_simple_json_resp_request (Context c, MemRef req_type, JsonBuilder *json)
    {
        if (req_type.equalTo("connect") || req_type.equalTo("disconnect")) {
            json->addSafeKeyVal("err", JsonUint32{0});
        }
        else if (req_type.equalTo("status")) {
            ThePrinterMain::get_json_status(c, json);
        }
        else {
            return false;
        }
        
        return true;
    }
    
    class UserClientState : public TheRequestInterface::RequestUserCallback {
    private:
        enum class State : uint8_t {
            NO_CLIENT,
            READ_OPEN, READ_WAIT, READ_READ,
            WRITE_OPEN, WRITE_WAIT, WRITE_WRITE, WRITE_EOF,
            JSONRESP_WAITBUF,
            FILES_OPEN, FILES_WAITBUF_HEAD, FILES_WAITBUF_ENTRY, FILES_REQUEST_ENTRY,
            DL_TEST, UL_TEST
        };
        
        enum class ResourceState : uint8_t  {NONE, FILE, DIRLISTER};
        
    private:
        void init (Context c)
        {
            m_state = State::NO_CLIENT;
            m_resource_state = ResourceState::NONE;
        }
        
        void deinit (Context c)
        {
            reset(c);
        }
        
        void reset (Context c)
        {
            if (m_resource_state == ResourceState::FILE) {
                m_buffered_file.deinit(c);
            }
            else if (m_resource_state == ResourceState::DIRLISTER) {
                m_dirlister.deinit(c);
            }
            m_state = State::NO_CLIENT;
            m_resource_state = ResourceState::NONE;
        }
        
        void complete_request (Context c)
        {
            AMBRO_ASSERT(m_state != State::NO_CLIENT)
            
            m_request->completeHandling(c);
            reset(c);
        }
        
        void accept_request_common (Context c, TheRequestInterface *request)
        {
            AMBRO_ASSERT(m_state == State::NO_CLIENT)
            AMBRO_ASSERT(m_resource_state == ResourceState::NONE)
            
            request->setCallback(c, this);
            m_request = request;
        }
        
        void init_file (Context c)
        {
            m_buffered_file.init(c, APRINTER_CB_OBJFUNC_T(&UserClientState::buffered_file_handler, this));
            m_resource_state = ResourceState::FILE;
        }
        
        void init_dirlister (Context c)
        {
            m_dirlister.init(c, APRINTER_CB_OBJFUNC_T(&UserClientState::dirlister_handler, this));
            m_resource_state = ResourceState::DIRLISTER;
        }
        
    public:
        void acceptGetFileRequest (Context c, TheRequestInterface *request, char const *file_path)
        {
            accept_request_common(c, request);
            
            m_file_path = file_path;
            m_state = State::READ_OPEN;
            init_file(c);
            m_buffered_file.startOpen(c, file_path, false, TheBufferedFile::OpenMode::OPEN_READ, WebRootPath());
        }
        
        void acceptUploadFileRequest (Context c, TheRequestInterface *request, char const *file_path)
        {
            accept_request_common(c, request);
            
            m_state = State::WRITE_OPEN;
            init_file(c);
            m_buffered_file.startOpen(c, file_path, false, TheBufferedFile::OpenMode::OPEN_WRITE, UploadBasePath());
        }
        
        void acceptJsonResponseRequest (Context c, TheRequestInterface *request, MemRef req_type)
        {
            accept_request_common(c, request);
            
            // Start delayed response adoption to wait for sufficient space in the send buffer.
            m_state = State::JSONRESP_WAITBUF;
            m_json_req.req_type = req_type;
            m_request->adoptResponseBody(c, true);
        }
        
        void acceptFilesRequest (Context c, TheRequestInterface *request, char const *dir_path, bool flag_dirs)
        {
            accept_request_common(c, request);
            
            m_state = State::FILES_OPEN;
            m_json_req.dir_path = dir_path;
            m_json_req.flag_dirs = flag_dirs;
            init_dirlister(c);
            m_dirlister.startOpen(c, dir_path, false, UploadBasePath());
        }
        
#if APRINTER_ENABLE_HTTP_TEST
        void acceptDownloadTestRequest (Context c, TheRequestInterface *request)
        {
            accept_request_common(c, request);
            
            m_request->setResponseContentType(c, "application/octet-stream");
            m_request->adoptResponseBody(c);
            m_state = State::DL_TEST;
        }
        
        void acceptUploadTestRequest (Context c, TheRequestInterface *request)
        {
            accept_request_common(c, request);
            
            m_request->adoptRequestBody(c);
            m_state = State::UL_TEST;
        }
#endif
        
    private:
        void requestTerminated (Context c)
        {
            AMBRO_ASSERT(m_state != State::NO_CLIENT)
            
            reset(c);
        }
        
        void requestBufferEvent (Context c)
        {
            switch (m_state) {
                case State::WRITE_WAIT: {
                    auto buf_st = m_request->getRequestBodyBufferState(c);
                    if (buf_st.length > 0) {
                        m_cur_chunk_size = MinValue(buf_st.data.wrap, buf_st.length);
                        m_buffered_file.startWriteData(c, buf_st.data.ptr1, m_cur_chunk_size);
                        m_state = State::WRITE_WRITE;
                    }
                    else if (buf_st.eof) {
                        m_buffered_file.startWriteEof(c);
                        m_state = State::WRITE_EOF;
                    }
                } break;
                
                case State::WRITE_WRITE:
                case State::WRITE_EOF:
                    break;
                
#if APRINTER_ENABLE_HTTP_TEST
                case State::UL_TEST: {
                    auto buf_st = m_request->getRequestBodyBufferState(c);
                    if (buf_st.length > 0) {
                        m_request->acceptRequestBodyData(c, buf_st.length);
                    }
                    else if (buf_st.eof) {
                        return complete_request(c);
                    }
                } break;
#endif
                
                default:
                    AMBRO_ASSERT(false);
            }
        }
        
        void responseBufferEvent (Context c)
        {
        again:
            switch (m_state) {
                case State::READ_WAIT: {
                    auto buf_st = m_request->getResponseBodyBufferState(c);
                    size_t allowed_length = MinValue(GetSdChunkSize, buf_st.length);
                    if (m_cur_chunk_size < allowed_length) {
                        auto dest_buf = buf_st.data.subFrom(m_cur_chunk_size);
                        m_buffered_file.startReadData(c, dest_buf.ptr1, MinValue(dest_buf.wrap, (size_t)(allowed_length - m_cur_chunk_size)));
                        m_state = State::READ_READ;
                    }
                } break;
                
                case State::READ_READ:
                    break;
                
                case State::JSONRESP_WAITBUF: {
                    if (m_request->getResponseBodyBufferAvailBeforeHeadSent(c) < JsonBufferSize) {
                        return;
                    }
                    
                    load_json_buffer(c);
                    
                    m_json_req.builder.start();
                    m_json_req.builder.startObject();
                    
                    if (!handle_simple_json_resp_request(c, m_json_req.req_type, &m_json_req.builder)) {
                        m_request->setResponseStatus(c, HttpStatusCodes::NotFound());
                        return complete_request(c);
                    }
                    
                    m_json_req.builder.endObject();
                    
                    if (!send_json_buffer(c, true)) {
                        m_request->setResponseStatus(c, HttpStatusCodes::InternalServerError());
                    }
                    
                    return complete_request(c);
                } break;
                
                case State::FILES_WAITBUF_HEAD: {
                    if (m_request->getResponseBodyBufferAvailBeforeHeadSent(c) < JsonBufferSize) {
                        return;
                    }
                    
                    load_json_buffer(c);
                    
                    m_json_req.builder.start();
                    m_json_req.builder.startObject();
                    m_json_req.builder.addSafeKeyVal("dir", JsonString{m_json_req.dir_path});
                    m_json_req.builder.addKeyArray(JsonSafeString{"files"});
                    
                    if (!send_json_buffer(c, true)) {
                        m_request->setResponseStatus(c, HttpStatusCodes::InternalServerError());
                        return complete_request(c);
                    }
                    
                    m_state = State::FILES_WAITBUF_ENTRY;
                    goto again;
                } break;
                
                case State::FILES_WAITBUF_ENTRY: {
                    if (m_request->getResponseBodyBufferState(c).length < JsonBufferSize) {
                        return;
                    }
                    
                    m_state = State::FILES_REQUEST_ENTRY;
                    m_dirlister.requestEntry(c);
                } break;
                
                case State::FILES_REQUEST_ENTRY:
                    break;
                
#if APRINTER_ENABLE_HTTP_TEST
                case State::DL_TEST: {
                    while (true) {
                        auto buf_st = m_request->getResponseBodyBufferState(c);
                        if (buf_st.length < GetSdChunkSize) {
                            break;
                        }
                        size_t len1 = MinValue(GetSdChunkSize, buf_st.data.wrap);
                        memset(buf_st.data.ptr1, 'X', len1);
                        if (len1 < GetSdChunkSize) {
                            memset(buf_st.data.ptr2, 'X', GetSdChunkSize-len1);
                        }
                        m_request->provideResponseBodyData(c, GetSdChunkSize);
                    }
                } break;
#endif
                
                default:
                    AMBRO_ASSERT(false);
            }
        }
        
        void buffered_file_handler (Context c, typename TheBufferedFile::Error error, size_t read_length)
        {
            AMBRO_ASSERT(m_resource_state == ResourceState::FILE)
            
            switch (m_state) {
                case State::READ_OPEN:
                case State::WRITE_OPEN: {
                    if (error != TheBufferedFile::Error::NO_ERROR) {
                        auto status = (error == TheBufferedFile::Error::NOT_FOUND) ? HttpStatusCodes::NotFound() : HttpStatusCodes::InternalServerError();
                        m_request->setResponseStatus(c, status);
                        return complete_request(c);
                    }
                    
                    if (m_state == State::READ_OPEN) {
                        m_request->setResponseContentType(c, get_content_type(m_file_path));
                        m_request->adoptResponseBody(c);
                        
                        m_state = State::READ_WAIT;
                        m_cur_chunk_size = 0;
                    } else {
                        m_request->adoptRequestBody(c);
                        
                        m_state = State::WRITE_WAIT;
                    }
                } break;
                
                case State::READ_READ: {
                    if (error != TheBufferedFile::Error::NO_ERROR) {
                        ThePrinterMain::print_pgm_string(c, AMBRO_PSTR("//HttpSdReadError\n"));
                        return complete_request(c);
                    }
                    
                    AMBRO_ASSERT(read_length <= GetSdChunkSize - m_cur_chunk_size)
                    m_cur_chunk_size += read_length;
                    
                    if (m_cur_chunk_size == GetSdChunkSize || (read_length == 0 && m_cur_chunk_size > 0)) {
                        m_request->provideResponseBodyData(c, m_cur_chunk_size);
                        m_cur_chunk_size = 0;
                    }
                    
                    if (read_length == 0) {
                        return complete_request(c);
                    }
                    
                    m_state = State::READ_WAIT;
                    responseBufferEvent(c);
                } break;
                
                case State::WRITE_WRITE:
                case State::WRITE_EOF: {
                    if (error != TheBufferedFile::Error::NO_ERROR) {
                        ThePrinterMain::print_pgm_string(c, AMBRO_PSTR("//HttpSdWriteError\n"));
                        m_request->setResponseStatus(c, HttpStatusCodes::InternalServerError());
                        return complete_request(c);
                    }
                    
                    if (m_state == State::WRITE_EOF) {
                        return complete_request(c);
                    }
                    
                    m_request->acceptRequestBodyData(c, m_cur_chunk_size);
                    
                    m_state = State::WRITE_WAIT;
                    requestBufferEvent(c);
                } break;
                
                default: AMBRO_ASSERT(false);
            }
        }
        
        void dirlister_handler (Context c, typename TheDirLister::Error error, char const *name, FsEntry entry)
        {
            AMBRO_ASSERT(m_resource_state == ResourceState::DIRLISTER)
            
            switch (m_state) {
                case State::FILES_OPEN: {
                    if (error != TheDirLister::Error::NO_ERROR) {
                        auto status = (error == TheDirLister::Error::NOT_FOUND) ? HttpStatusCodes::NotFound() : HttpStatusCodes::InternalServerError();
                        m_request->setResponseStatus(c, status);
                        return complete_request(c);
                    }
                    
                    m_request->adoptResponseBody(c, true);
                    m_state = State::FILES_WAITBUF_HEAD;
                } break;
                
                case State::FILES_REQUEST_ENTRY: {
                    if (error != TheDirLister::Error::NO_ERROR) {
                        return complete_request(c);
                    }
                    
                    if (name && name[0] == '.') {
                        m_dirlister.requestEntry(c);
                        return;
                    }
                    
                    load_json_buffer(c);
                    
                    if (name) {
                        m_json_req.builder.beginString();
                        if (m_json_req.flag_dirs && entry.getType() == EntryType::DIR_TYPE) {
                            m_json_req.builder.addStringChar('*');
                        }
                        m_json_req.builder.addStringMem(name);
                        m_json_req.builder.endString();
                    } else {
                        m_json_req.builder.endArray();
                        m_json_req.builder.endObject();
                    }
                    
                    if (!send_json_buffer(c, false) || !name) {
                        return complete_request(c);
                    }
                    
                    m_state = State::FILES_WAITBUF_ENTRY;
                    responseBufferEvent(c);
                } break;
                
                default: AMBRO_ASSERT(false);
            }
        }
        
        void load_json_buffer (Context c)
        {
            auto *o = Object::self(c);
            
            m_json_req.builder.loadBuffer(o->json_buffer, sizeof(o->json_buffer));
        }
        
        bool send_json_buffer (Context c, bool initial)
        {
            auto *o = Object::self(c);
            
            size_t length = m_json_req.builder.getLength();
            if (length > JsonBufferSize) {
                return false;
            }
            
            if (initial) {
                m_request->setResponseContentType(c, "application/json");
                m_request->adoptResponseBody(c);
            }
            
            auto buf_st = m_request->getResponseBodyBufferState(c);
            AMBRO_ASSERT(buf_st.length >= JsonBufferSize)
            
            buf_st.data.copyIn(MemRef(o->json_buffer, length));
            m_request->provideResponseBodyData(c, length);
            
            return true;
        }
        
    private:
        TheRequestInterface *m_request;
        State m_state;
        ResourceState m_resource_state;
        union {
            TheBufferedFile m_buffered_file;
            TheDirLister m_dirlister;
        };
        union {
            struct {
                char const *m_file_path;
                size_t m_cur_chunk_size;
            };
            struct {
                MemRef req_type;
                JsonBuilder builder;
                char const *dir_path;
                bool flag_dirs;
            } m_json_req;
        };
    };
    
public:
    struct Object : public ObjBase<WebInterfaceModule, ParentObject, MakeTypeList<
        TheHttpServer
    >> {
        char json_buffer[JsonBufferSize + 2];
    };
};

APRINTER_ALIAS_STRUCT_EXT(WebInterfaceModuleService, (
    APRINTER_AS_TYPE(HttpServerNetParams),
    APRINTER_AS_VALUE(int, JsonBufferSize)
), (
    APRINTER_MODULE_TEMPLATE(WebInterfaceModuleService, WebInterfaceModule)
))

#include <aprinter/EndNamespace.h>

#endif
