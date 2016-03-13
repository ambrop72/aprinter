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
#include <stdint.h>
#include <string.h>

#include <aprinter/meta/WrapFunction.h>
#include <aprinter/meta/MinMax.h>
#include <aprinter/meta/AliasStruct.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/ProgramMemory.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/OneOf.h>
#include <aprinter/base/MemRef.h>
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
    using TimeType = typename Context::Clock::TimeType;
    using TheCommandStream = typename ThePrinterMain::CommandStream;
    
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
    
    static int const NumGcodeSlots = Params::NumGcodeSlots;
    static_assert(NumGcodeSlots > 0, "");
    
    static size_t const MaxGcodeCommandSize = Params::MaxGcodeCommandSize;
    static_assert(MaxGcodeCommandSize >= 32, "");
    
    using TheGcodeParser = typename Params::TheGcodeParserService::template Parser<Context, size_t, typename ThePrinterMain::FpType>;
    
    static_assert(TheHttpServer::MaxTxChunkSize >= ThePrinterMain::CommandSendBufClearance, "HTTP/TCP send buffer is too small");
    
    static TimeType const GcodeSendBufTimeoutTicks = Params::GcodeSendBufTimeout::value() * Context::Clock::time_freq;
    
    static size_t const GcodeParseChunkSize = 16;
    
    static constexpr char const * WebRootPath() { return "www"; }
    static constexpr char const * IndexPage() { return "reprap.htm"; }
    static constexpr char const * UploadBasePath() { return nullptr; }
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        for (int i = 0; i < NumGcodeSlots; i++) {
            o->gcode_slots[i].init(c);
        }
        
        TheHttpServer::init(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        
        TheHttpServer::deinit(c);
        
        // Note, do this after HttpServed deinit so that it has now detached
        // from any slots it was using.
        for (int i = 0; i < NumGcodeSlots; i++) {
            o->gcode_slots[i].deinit(c);
        }
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
            
            if (path.equalTo("/rr_gcode")) {
                GcodeSlot *gcode_slot = find_available_gcode_slot(c);
                if (!gcode_slot) {
                    request->setResponseStatus(c, HttpStatusCodes::ServiceUnavailable());
                    goto error;
                }
                
                return state->acceptGcodeRequest(c, request, gcode_slot);
            }
            
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
    
    class GcodeSlot;
    
    class UserClientState : public TheRequestInterface::RequestUserCallback {
        friend class GcodeSlot;
        
    private:
        enum class State : uint8_t {
            NO_CLIENT,
            READ_OPEN, READ_WAIT, READ_READ,
            WRITE_OPEN, WRITE_WAIT, WRITE_WRITE, WRITE_EOF,
            JSONRESP_WAITBUF,
            FILES_OPEN, FILES_WAITBUF_HEAD, FILES_WAITBUF_ENTRY, FILES_REQUEST_ENTRY,
            GCODE,
            DL_TEST, UL_TEST
        };
        
        enum class ResourceState : uint8_t {NONE, FILE, DIRLISTER, GCODE_SLOT};
        
    public:
        void init (Context c)
        {
            m_state = State::NO_CLIENT;
            m_resource_state = ResourceState::NONE;
        }
        
        void deinit (Context c)
        {
            reset(c);
        }
        
    private:
        void reset (Context c)
        {
            switch (m_resource_state) {
                case ResourceState::FILE:       m_buffered_file.deinit(c); break;
                case ResourceState::DIRLISTER:  m_dirlister.deinit(c);     break;
                case ResourceState::GCODE_SLOT: m_gcode_slot->detach(c);   break;
                default: break;
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
        
        void init_gcode_slot (Context c, GcodeSlot *gcode_slot)
        {
            m_gcode_slot = gcode_slot;
            m_gcode_slot->attach(c, this);
            m_resource_state = ResourceState::GCODE_SLOT;
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
        
        void acceptGcodeRequest (Context c, TheRequestInterface *request, GcodeSlot *gcode_slot)
        {
            accept_request_common(c, request);
            
            m_state = State::GCODE;
            init_gcode_slot(c, gcode_slot);
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
                
                case State::GCODE:
                    return m_gcode_slot->requestBufferEvent(c);
                
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
                
                case State::GCODE:
                    return m_gcode_slot->responseBufferEvent(c);
                
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
            
            if (length > 0) {
                buf_st.data.copyIn(MemRef(o->json_buffer, length));
                m_request->provideResponseBodyData(c, length);
            }
            
            return true;
        }
        
    private:
        TheRequestInterface *m_request;
        State m_state;
        ResourceState m_resource_state;
        union {
            TheBufferedFile m_buffered_file;
            TheDirLister m_dirlister;
            GcodeSlot *m_gcode_slot;
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
    
    static GcodeSlot * find_available_gcode_slot (Context c)
    {
        auto *o = Object::self(c);
        
        for (int i = 0; i < NumGcodeSlots; i++) {
            if (o->gcode_slots[i].isAvailable(c)) {
                return &o->gcode_slots[i];
            }
        }
        return nullptr;
    }
    
    class GcodeSlot : private ThePrinterMain::CommandStreamCallback {
    private:
        enum class State : uint8_t {AVAILABLE, ATTACHED, FINISHING};
        
    public:
        void init (Context c)
        {
            m_next_event.init(c, APRINTER_CB_OBJFUNC_T(&GcodeSlot::next_event_handler, this));
            m_send_buf_check_event.init(c, APRINTER_CB_OBJFUNC_T(&GcodeSlot::send_buf_check_event_handler, this));
            m_send_buf_timeout_event.init(c, APRINTER_CB_OBJFUNC_T(&GcodeSlot::send_buf_timeout_event_handler, this));
            m_state = State::AVAILABLE;
        }
        
        void deinit (Context c)
        {
            if (m_state != State::AVAILABLE) {
                m_command_stream.deinit(c);
                m_gcode_parser.deinit(c);
            }
            
            m_send_buf_timeout_event.deinit(c);
            m_send_buf_check_event.deinit(c);
            m_next_event.deinit(c);
        }
        
        bool isAvailable (Context c)
        {
            return (m_state == State::AVAILABLE);
        }
        
        void attach (Context c, UserClientState *client)
        {
            AMBRO_ASSERT(m_state == State::AVAILABLE)
            
            m_gcode_parser.init(c);
            m_command_stream.init(c, this);
            
            m_state = State::ATTACHED;
            m_client = client;
            m_buffer_pos = 0;
            m_output_pos = 0;
            m_send_buf_request = 0;
            
            m_client->m_request->adoptRequestBody(c);
            m_client->m_request->adoptResponseBody(c);
        }
        
        void detach (Context c)
        {
            AMBRO_ASSERT(m_state == State::ATTACHED)
            
            if (m_command_stream.tryCancelCommand(c)) {
                reset(c);
            } else {
                m_state = State::FINISHING;
                if (m_send_buf_request > 0) {
                    m_send_buf_check_event.prependNow(c);
                }
            }
        }
        
        void requestBufferEvent (Context c)
        {
            AMBRO_ASSERT(m_state == State::ATTACHED)
            
            if (!m_command_stream.hasCommand(c)) {
                m_next_event.prependNow(c);
            }
        }
        
        void responseBufferEvent (Context c)
        {
            AMBRO_ASSERT(m_state == State::ATTACHED)
            
            if (m_send_buf_request > 0) {
                m_send_buf_check_event.prependNow(c);
            }
        }
        
    private:
        void reset (Context c)
        {
            AMBRO_ASSERT(m_state == OneOf(State::ATTACHED, State::FINISHING))
            
            m_command_stream.deinit(c);
            m_gcode_parser.deinit(c);
            
            m_state = State::AVAILABLE;
            m_next_event.unset(c);
            m_send_buf_check_event.unset(c);
            m_send_buf_timeout_event.unset(c);
        }
        
        void next_event_handler (Context c)
        {
            AMBRO_ASSERT(m_state == OneOf(State::ATTACHED, State::FINISHING))
            AMBRO_ASSERT(!m_command_stream.hasCommand(c))
            
            if (m_state == State::FINISHING) {
                return reset(c);
            }
            
            if (!m_gcode_parser.haveCommand(c)) {
                m_gcode_parser.startCommand(c, m_buffer, 0);
            }
            
            while (true) {
                bool line_buffer_exhausted = (m_buffer_pos == MaxGcodeCommandSize);
                
                if (m_gcode_parser.extendCommand(c, m_buffer_pos, line_buffer_exhausted)) {
                    return m_command_stream.startCommand(c, &m_gcode_parser);
                }
                
                if (line_buffer_exhausted) {
                    m_command_stream.setAcceptMsg(c, false);
                    ThePrinterMain::print_pgm_string(c, AMBRO_PSTR("//HttpGcodeLineTooLong\n"));
                    return m_client->complete_request(c);
                }
                
                auto buf_st = m_client->m_request->getRequestBodyBufferState(c);
                if (buf_st.length == 0) {
                    if (buf_st.eof) {
                        return m_client->complete_request(c);
                    }
                    break;
                }
                
                size_t to_copy = MinValue(GcodeParseChunkSize, MinValue(buf_st.length, (size_t)(MaxGcodeCommandSize - m_buffer_pos)));
                buf_st.data.copyOut(MemRef(m_buffer + m_buffer_pos, to_copy));
                m_buffer_pos += to_copy;
                m_client->m_request->acceptRequestBodyData(c, to_copy);
            }
        }
        
        void send_buf_check_event_handler (Context c)
        {
            AMBRO_ASSERT(m_state == OneOf(State::ATTACHED, State::FINISHING))
            AMBRO_ASSERT(m_send_buf_request > 0)
            
            bool have = true;
            if (m_state == State::ATTACHED) {
                auto buf_st = m_client->m_request->getResponseBodyBufferState(c);
                AMBRO_ASSERT(buf_st.length >= m_output_pos)
                have = (buf_st.length - m_output_pos >= m_send_buf_request);
            }
            
            if (have) {
                m_send_buf_request = 0;
                m_send_buf_timeout_event.unset(c);
                return m_command_stream.reportSendBufEventDirectly(c);
            }
        }
        
        void send_buf_timeout_event_handler (Context c)
        {
            AMBRO_ASSERT(m_state == OneOf(State::ATTACHED, State::FINISHING))
            AMBRO_ASSERT(m_send_buf_request > 0)
            
            // This normally will not happen in FINISHING state because in that
            // case we report send buffer to always be available. But there might
            // still be a theoretical possibility to find ourselves in FINISHING
            // here, so be robust.
            if (m_state == State::ATTACHED) {
                m_command_stream.setAcceptMsg(c, false);
                ThePrinterMain::print_pgm_string(c, AMBRO_PSTR("//HttpGcodeSendBufTimeout\n"));
                
                return m_client->complete_request(c);
            }
        }
        
        void finish_command_impl (Context c)
        {
            AMBRO_ASSERT(m_state == OneOf(State::ATTACHED, State::FINISHING))
            
            if (m_state == State::ATTACHED) {
                size_t cmd_len = m_gcode_parser.getLength(c);
                AMBRO_ASSERT(cmd_len <= m_buffer_pos)
                m_buffer_pos -= cmd_len;
                memmove(m_buffer, m_buffer + cmd_len, m_buffer_pos);
            }
            
            m_next_event.prependNowNotAlready(c);
        }
        
        void reply_poke_impl (Context c)
        {
            AMBRO_ASSERT(m_state == OneOf(State::ATTACHED, State::FINISHING))
            
            if (m_state == State::ATTACHED && m_output_pos > 0) {
                m_client->m_request->provideResponseBodyData(c, m_output_pos);
            }
            m_output_pos = 0;
        }
        
        void reply_append_buffer_impl (Context c, char const *str, size_t length)
        {
            AMBRO_ASSERT(m_state == OneOf(State::ATTACHED, State::FINISHING))
            
            if (m_state == State::ATTACHED && length > 0) {
                auto buf_st = m_client->m_request->getResponseBodyBufferState(c);
                AMBRO_ASSERT(buf_st.length >= m_output_pos)
                size_t to_copy = MinValue(buf_st.length - m_output_pos, length);
                buf_st.data.subFrom(m_output_pos).copyIn(MemRef(str, to_copy));
                m_output_pos += to_copy;
            }
        }
        
        bool have_send_buf_impl (Context c, size_t length)
        {
            AMBRO_ASSERT(m_state == OneOf(State::ATTACHED, State::FINISHING))
            
            if (m_state != State::ATTACHED) {
                return true;
            }
            auto buf_st = m_client->m_request->getResponseBodyBufferState(c);
            AMBRO_ASSERT(buf_st.length >= m_output_pos)
            return (buf_st.length - m_output_pos >= length);
        }
        
        bool request_send_buf_event_impl (Context c, size_t length)
        {
            AMBRO_ASSERT(m_state == OneOf(State::ATTACHED, State::FINISHING))
            AMBRO_ASSERT(m_send_buf_request == 0)
            AMBRO_ASSERT(length > 0)
            
            if (length > TheHttpServer::MaxTxChunkSize - m_output_pos) {
                return false;
            }
            m_send_buf_request = length;
            m_send_buf_check_event.prependNowNotAlready(c);
            m_send_buf_timeout_event.appendAfter(c, GcodeSendBufTimeoutTicks);
            return true;
        }
        
        void cancel_send_buf_event_impl (Context c)
        {
            AMBRO_ASSERT(m_state == OneOf(State::ATTACHED, State::FINISHING))
            AMBRO_ASSERT(m_send_buf_request > 0)
            
            m_send_buf_request = 0;
            m_send_buf_check_event.unset(c);
            m_send_buf_timeout_event.unset(c);
        }
        
    private:
        UserClientState *m_client;
        typename Context::EventLoop::QueuedEvent m_next_event;
        typename Context::EventLoop::QueuedEvent m_send_buf_check_event;
        typename Context::EventLoop::TimedEvent m_send_buf_timeout_event;
        TheGcodeParser m_gcode_parser;
        TheCommandStream m_command_stream;
        size_t m_buffer_pos;
        size_t m_output_pos;
        size_t m_send_buf_request;
        State m_state;
        char m_buffer[MaxGcodeCommandSize];
    };
    
public:
    struct Object : public ObjBase<WebInterfaceModule, ParentObject, MakeTypeList<
        TheHttpServer
    >> {
        GcodeSlot gcode_slots[NumGcodeSlots];
        char json_buffer[JsonBufferSize + 2];
    };
};

APRINTER_ALIAS_STRUCT_EXT(WebInterfaceModuleService, (
    APRINTER_AS_TYPE(HttpServerNetParams),
    APRINTER_AS_VALUE(size_t, JsonBufferSize),
    APRINTER_AS_VALUE(int, NumGcodeSlots),
    APRINTER_AS_TYPE(TheGcodeParserService),
    APRINTER_AS_VALUE(size_t, MaxGcodeCommandSize),
    APRINTER_AS_TYPE(GcodeSendBufTimeout)
), (
    APRINTER_MODULE_TEMPLATE(WebInterfaceModuleService, WebInterfaceModule)
))

#include <aprinter/EndNamespace.h>

#endif
