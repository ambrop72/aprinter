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
#include <aprinter/meta/ServiceUtils.h>
#include <aprinter/meta/ListForEach.h>
#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/meta/FuncUtils.h>
#include <aprinter/meta/BasicMetaUtils.h>
#include <aprinter/meta/MemberType.h>
#include <aprinter/base/Object.h>
#include <aprinter/base/ProgramMemory.h>
#include <aprinter/base/Assert.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/OneOf.h>
#include <aprinter/base/MemRef.h>
#include <aprinter/net/http/HttpServer.h>
#include <aprinter/fs/BufferedFile.h>
#include <aprinter/misc/StringTools.h>
#include <aprinter/printer/ServiceList.h>
#include <aprinter/printer/utils/JsonBuilder.h>
#include <aprinter/printer/utils/ConvenientCommandStream.h>
#include <aprinter/printer/utils/WebRequest.h>
#include <aprinter/printer/utils/ModuleUtils.h>

#define APRINTER_ENABLE_HTTP_TEST 1

namespace APrinter {

template <typename ModuleArg>
class WebInterfaceModule {
    APRINTER_UNPACK_MODULE_ARG(ModuleArg)
    
public:
    struct Object;
    
private:
    AMBRO_DECLARE_GET_MEMBER_TYPE_FUNC(GetMemberType_WebApiRequestHandlers, WebApiRequestHandlers)
    
private:
    static constexpr char const * WebRootPath() { return "www"; }
    static constexpr char const * RootAccessPath() { return "/sdcard/"; }
    static constexpr char const * IndexPage() { return "reprap.htm"; }
    static constexpr char const * UploadBasePath() { return nullptr; }
    
    using TheHttpServerService = HttpServerService<
        typename Params::HttpServerNetParams,
        128,   // MaxRequestLineLength
        64,    // MaxHeaderLineLength
        283,   // ExpectedResponseLength
        10000, // MaxRequestHeadLength
        256,   // MaxChunkHeaderLength
        1024,  // MaxTrailerLength
        4      // MaxQueryParams
    >;
    
    static size_t const GetSdChunkSize = 512;
    static size_t const GcodeParseChunkSize = 16;
    
private:
    using TimeType = typename Context::Clock::TimeType;
    
    static size_t const JsonBufferSize = Params::JsonBufferSize;
    static_assert(JsonBufferSize >= 128, "");
    
    // Make these settings available to WebRequest handlers.
    struct WebApiConfig {
        static size_t const JsonBufferSize = WebInterfaceModule::JsonBufferSize;
        static constexpr char const * UploadBasePath() { return WebInterfaceModule::UploadBasePath(); }
    };
    
    // Find all modules claiming to provide request handlers.
    // The result is a list of TypeDictEntry<WrapInt<ModuleIndex>, ServiceDefinition<...>>.
    using WebApiHandlerProviders = typename ThePrinterMain::template GetServiceProviders<ServiceList::WebApiHandlerService>;
    
    // Make a list of all the WebApi template classes in the modules that implement request handlers.
    template <typename Provider>
    using GetProviderWebApi = typename ThePrinterMain::template GetProviderModule<Provider>::template WebApi<WebApiConfig>;
    using WebApis = MapTypeList<WebApiHandlerProviders, TemplateFunc<GetProviderWebApi>>;
    
    // Make a list of all request handler classes, by joining the lists
    // of request handlers for different providers.
    using WebApiHandlers = JoinTypeListList<MapTypeList<WebApis, GetMemberType_WebApiRequestHandlers>>;
    
    // Calculate the maximum handler size.
    // template <typename List, typename InitialValue, template<typename ListElem, typename AccumValue> class FoldFunc>
    template <typename RequestHandler, typename AccumValue>
    using MaxSizeFoldFunc = WrapValue<size_t, MaxValue(sizeof(RequestHandler), AccumValue::Value)>;
    static size_t const CustomHandlerMaxSize = TypeListFold<WebApiHandlers, WrapValue<size_t, 1>, MaxSizeFoldFunc>::Value;
    
    // Calculate the maximum handler alignment.
    template <typename RequestHandler, typename AccumValue>
    using MaxAlignFoldFunc = WrapValue<size_t, MaxValue(alignof(RequestHandler), AccumValue::Value)>;
    static size_t const CustomHandlerMaxAlign = TypeListFold<WebApiHandlers, WrapValue<size_t, 1>, MaxAlignFoldFunc>::Value;
    
    struct HttpRequestHandler;
    class UserClientState;
    APRINTER_MAKE_INSTANCE(TheHttpServer, (TheHttpServerService::template Server<Context, Object, ThePrinterMain, HttpRequestHandler, UserClientState>))
    using TheRequestInterface = typename TheHttpServer::TheRequestInterface;
    
    using TheFsAccess = typename ThePrinterMain::template GetFsAccess<>;
    using TheBufferedFile = BufferedFile<Context, TheFsAccess>;
    
    using TheWebRequest = WebRequest<Context>;
    using TheWebRequestCallback = WebRequestCallback<Context>;
    
    static int const NumGcodeSlots = Params::NumGcodeSlots;
    static_assert(NumGcodeSlots > 0, "");
    
    static size_t const MaxGcodeCommandSize = Params::MaxGcodeCommandSize;
    static_assert(MaxGcodeCommandSize >= 32, "");
    
    using TheConvenientStream = ConvenientCommandStream<Context, ThePrinterMain>;
    
    using TheGcodeParser = typename Params::TheGcodeParserService::template Parser<Context, size_t, typename ThePrinterMain::FpType>;
    
    static_assert(TheHttpServer::MaxTxChunkOverhead <= 255, "");
    static_assert(TheHttpServer::GuaranteedTxChunkSizeWithoutPoke >= ThePrinterMain::CommandSendBufClearance,
                  "HTTP send buffer too small for send buffer clearance");
    static_assert(TheHttpServer::GuaranteedTxChunkSizeBeforeHead >= JsonBufferSize, "HTTP send buffer too small for JsonBufferSize");
    static_assert(TheHttpServer::GuaranteedTxChunkSizeWithoutPoke >= JsonBufferSize, "HTTP send buffer too small for JsonBufferSize");
    static_assert(TheHttpServer::GuaranteedTxChunkSizeWithoutPoke >= GetSdChunkSize, "HTTP send buffer too small for SD card transfer");
    
    static TimeType const GcodeSendBufTimeoutTicks = Params::GcodeSendBufTimeout::value() * Context::Clock::time_freq;
    
public:
    static void init (Context c)
    {
        auto *o = Object::self(c);
        
        for (GcodeSlot &slot : o->gcode_slots) {
            slot.init(c);
        }
        
        TheHttpServer::init(c);
    }
    
    static void deinit (Context c)
    {
        auto *o = Object::self(c);
        
        TheHttpServer::deinit(c);
        
        // Note, do this after HttpServer deinit so that it has now detached
        // from any slots it was using.
        for (GcodeSlot &slot : o->gcode_slots) {
            slot.deinit(c);
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
            
            if (path.removePrefix("/rr_")) {
                return state->acceptJsonResponseRequest(c, request, path);
            }
            
            if (path.ptr[0] == '/') {
                char const *base_dir = WebRootPath();
                char const *file_path;
                if (path.equalTo("/")) {
                    file_path = IndexPage();
                } else if (path.removePrefix(RootAccessPath())) {
                    base_dir = nullptr;
                    file_path = path.ptr;
                } else {
                    file_path = path.ptr + 1;
                }
                return state->acceptGetFileRequest(c, request, file_path, base_dir);
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
    
    class UserClientState
    : private TheRequestInterface::RequestUserCallback,
      private TheWebRequest
    {
        friend class GcodeSlot;
        
    private:
        enum class State : uint8_t {
            NO_CLIENT,
            READ_OPEN, READ_WAIT, READ_READ,
            WRITE_OPEN, WRITE_WAIT, WRITE_WRITE, WRITE_EOF,
            JSONRESP_WAITBUF, JSONRESP_CUSTOM_TRY, JSONRESP_CUSTOM,
            GCODE,
            DL_TEST, UL_TEST
        };
        
        enum class ResourceState : uint8_t {NONE, FILE, GCODE_SLOT, CUSTOM_REQ};
        
    public:
        void init (Context c)
        {
            m_state = State::NO_CLIENT;
            m_resource_state = ResourceState::NONE;
        }
        
        void deinit (Context c)
        {
            switch (m_resource_state) {
                case ResourceState::NONE: break;
                case ResourceState::FILE:       m_buffered_file.deinit(c);                     break;
                case ResourceState::GCODE_SLOT: m_gcode_slot->detach(c);                       break;
                case ResourceState::CUSTOM_REQ: m_custom_req.callback->cbRequestTerminated(c); break;
                default: AMBRO_ASSERT(false);
            }
        }
        
    private:
        void reset (Context c)
        {
            deinit(c);
            init(c);
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
        
    public:
        void acceptGetFileRequest (Context c, TheRequestInterface *request, char const *file_path, char const *base_dir)
        {
            accept_request_common(c, request);
            
            m_file_path = file_path;
            m_state = State::READ_OPEN;
            init_file(c);
            m_buffered_file.startOpen(c, file_path, false, TheBufferedFile::OpenMode::OPEN_READ, base_dir);
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
            m_json_req.resp_body_pending = true;
            m_request->adoptResponseBody(c, true);
            m_request->controlResponseBodyTimeout(c, true);
        }
        
        void acceptGcodeRequest (Context c, TheRequestInterface *request, GcodeSlot *gcode_slot)
        {
            accept_request_common(c, request);
            
            m_state = State::GCODE;
            m_gcode_slot = gcode_slot;
            m_gcode_slot->attach(c, this);
            m_resource_state = ResourceState::GCODE_SLOT;
        }
        
#if APRINTER_ENABLE_HTTP_TEST
        void acceptDownloadTestRequest (Context c, TheRequestInterface *request)
        {
            accept_request_common(c, request);
            
            m_request->setResponseContentType(c, "application/octet-stream");
            m_request->adoptResponseBody(c);
            m_state = State::DL_TEST;
            m_request->controlResponseBodyTimeout(c, true);
        }
        
        void acceptUploadTestRequest (Context c, TheRequestInterface *request)
        {
            accept_request_common(c, request);
            
            m_request->adoptRequestBody(c);
            m_state = State::UL_TEST;
            m_request->controlRequestBodyTimeout(c, true);
        }
#endif
        
    private:
        void requestTerminated (Context c) override
        {
            AMBRO_ASSERT(m_state != State::NO_CLIENT)
            
            reset(c);
        }
        
        void requestBufferEvent (Context c) override
        {
            switch (m_state) {
                case State::WRITE_WAIT: {
                    auto buf_st = m_request->getRequestBodyBufferState(c);
                    if (buf_st.length > 0) {
                        m_cur_chunk_size = MinValue(buf_st.data.wrap, buf_st.length);
                        m_buffered_file.startWriteData(c, buf_st.data.ptr1, m_cur_chunk_size);
                        m_state = State::WRITE_WRITE;
                        m_request->controlRequestBodyTimeout(c, false);
                    }
                    else if (buf_st.eof) {
                        m_buffered_file.startWriteEof(c);
                        m_state = State::WRITE_EOF;
                        m_request->controlRequestBodyTimeout(c, false);
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
                        m_request->controlRequestBodyTimeout(c, true);
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
        
        void responseBufferEvent (Context c) override
        {
            switch (m_state) {
                case State::READ_WAIT: {
                    auto buf_st = m_request->getResponseBodyBufferState(c);
                    size_t allowed_length = MinValue(GetSdChunkSize, buf_st.length);
                    if (m_cur_chunk_size < allowed_length) {
                        auto dest_buf = buf_st.data.subFrom(m_cur_chunk_size);
                        m_buffered_file.startReadData(c, dest_buf.ptr1, MinValue(dest_buf.wrap, (size_t)(allowed_length - m_cur_chunk_size)));
                        m_state = State::READ_READ;
                        m_request->controlResponseBodyTimeout(c, false);
                    }
                } break;
                
                case State::READ_READ:
                    break;
                
                case State::JSONRESP_WAITBUF: {
                    if (m_request->getResponseBodyBufferAvailBeforeHeadSent(c) < JsonBufferSize) {
                        return;
                    }
                    
                    m_request->controlResponseBodyTimeout(c, false);
                    
                    load_json_buffer(c);
                    m_json_req.builder.start();
                    m_json_req.builder.startObject();
                    
                    if (handle_simple_json_resp_request(c, m_json_req.req_type, &m_json_req.builder)) {
                        m_json_req.builder.endObject();
                        if (!send_json_buffer(c)) {
                            m_request->setResponseStatus(c, HttpStatusCodes::InternalServerError());
                        }
                        return complete_request(c);
                    }
                    
                    AMBRO_ASSERT(m_resource_state == ResourceState::NONE)
                    m_state = State::JSONRESP_CUSTOM_TRY;
                    m_custom_req.callback = nullptr;
                    
                    bool not_handled = ListForBreak<WebApis>([&] (auto webapi_arg) {
                        using webapi = typename decltype(webapi_arg)::Type;
                        if (!webapi::handle_web_request(c, m_json_req.req_type, static_cast<TheWebRequest *>(this))) {
                            // Request was accepted, stop trying to dispatch.
                            return false;
                        }
                        AMBRO_ASSERT(m_state == State::JSONRESP_CUSTOM_TRY)
                        return true;
                    });
                    
                    if (!not_handled) {
                        // One of the modules claimed to have accepted the request.
                        // Let's trust it and not perform checks/asserts here, considering that
                        // the request might have been completed already.
                        return;
                    }
                    
                    m_request->setResponseStatus(c, HttpStatusCodes::NotFound());
                    return complete_request(c);
                } break;
                
                case State::JSONRESP_CUSTOM: {
                    if (m_json_req.custom_waiting) {
                        size_t avail = m_json_req.resp_body_pending ? m_request->getResponseBodyBufferAvailBeforeHeadSent(c) : m_request->getResponseBodyBufferState(c).length;
                        if (avail >= JsonBufferSize) {
                            m_json_req.custom_waiting = false;
                            m_request->controlResponseBodyTimeout(c, false);
                            return m_custom_req.callback->cbJsonBufferAvailable(c);
                        }
                    }
                } break;
                
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
                        m_request->controlResponseBodyTimeout(c, true);
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
                        m_request->controlResponseBodyTimeout(c, true);
                    } else {
                        m_request->adoptRequestBody(c);
                        
                        m_state = State::WRITE_WAIT;
                        m_request->controlRequestBodyTimeout(c, true);
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
                    m_request->controlResponseBodyTimeout(c, true);
                    m_request->pokeResponseBodyBufferEvent(c);
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
                    m_request->controlRequestBodyTimeout(c, true);
                    m_request->pokeRequestBodyBufferEvent(c);
                } break;
                
                default: AMBRO_ASSERT(false);
            }
        }
        
        void load_json_buffer (Context c)
        {
            auto *o = Object::self(c);
            m_json_req.builder.loadBuffer(o->json_buffer, sizeof(o->json_buffer));
        }
        
        bool send_json_buffer (Context c)
        {
            auto *o = Object::self(c);
            
            size_t length = m_json_req.builder.getLength();
            if (length > JsonBufferSize) {
                ThePrinterMain::print_pgm_string(c, AMBRO_PSTR("//HttpJsonBufOverrun\n"));
                return false;
            }
            
            if (m_json_req.resp_body_pending) {
                m_json_req.resp_body_pending = false;
                m_request->setResponseContentType(c, "application/json");
                m_request->adoptResponseBody(c);
            }
            
            auto buf_st = m_request->getResponseBodyBufferState(c);
            if (buf_st.length < length) {
                ThePrinterMain::print_pgm_string(c, AMBRO_PSTR("//HttpJsonTxOverrun\n"));
                return false;
            }
            
            if (length > 0) {
                buf_st.data.copyIn(MemRef(o->json_buffer, length));
                m_request->provideResponseBodyData(c, length);
            }
            
            return true;
        }
        
    private:
        MemRef getPath (Context c) override
        {
            AMBRO_ASSERT(m_state == OneOf(State::JSONRESP_CUSTOM_TRY, State::JSONRESP_CUSTOM))
            return m_request->getPath(c);
        }
        
        bool getParam (Context c, MemRef name, MemRef *value=nullptr) override
        {
            AMBRO_ASSERT(m_state == OneOf(State::JSONRESP_CUSTOM_TRY, State::JSONRESP_CUSTOM))
            return m_request->getParam(c, name, value);
        }
        
        void * doAcceptRequest (Context c, size_t state_size, size_t state_align) override
        {
            AMBRO_ASSERT(m_state == State::JSONRESP_CUSTOM_TRY)
            AMBRO_ASSERT(m_resource_state == ResourceState::NONE)
            AMBRO_ASSERT(state_size <= sizeof(m_custom_req.state))
            AMBRO_ASSERT(CustomHandlerMaxAlign % state_align == 0)
            
            m_resource_state = ResourceState::CUSTOM_REQ;
            return m_custom_req.state;
        }
        
        void doCompleteHandling (Context c, char const *http_status) override
        {
            AMBRO_ASSERT(m_state == OneOf(State::JSONRESP_CUSTOM_TRY, State::JSONRESP_CUSTOM))
            AMBRO_ASSERT(m_state != State::JSONRESP_CUSTOM_TRY || m_resource_state == ResourceState::NONE)
            
            if (http_status && m_json_req.resp_body_pending) {
                m_request->setResponseStatus(c, http_status);
            }
            return complete_request(c);
        }
        
        void doSetCallback (Context c, TheWebRequestCallback *callback) override
        {
            AMBRO_ASSERT(m_state == State::JSONRESP_CUSTOM_TRY)
            AMBRO_ASSERT(m_resource_state == ResourceState::CUSTOM_REQ)
            AMBRO_ASSERT(!m_custom_req.callback)
            AMBRO_ASSERT(callback)
            
            m_state = State::JSONRESP_CUSTOM;
            m_custom_req.callback = callback;
            m_json_req.custom_waiting = false;
            m_json_req.builder.start();
        }
        
        void doWaitForJsonBuffer (Context c) override
        {
            AMBRO_ASSERT(m_state == State::JSONRESP_CUSTOM)
            AMBRO_ASSERT(!m_json_req.custom_waiting)
            
            m_json_req.custom_waiting = true;
            m_request->controlResponseBodyTimeout(c, true);
            m_request->pokeResponseBodyBufferEvent(c);
        }
        
        JsonBuilder * doStartJson (Context c) override
        {
            AMBRO_ASSERT(m_state == State::JSONRESP_CUSTOM)
            
            load_json_buffer(c);
            return &m_json_req.builder;
        }
        
        bool doEndJson (Context c) override
        {
            AMBRO_ASSERT(m_state == State::JSONRESP_CUSTOM)
            
            return send_json_buffer(c);
        }
        
    private:
        TheRequestInterface *m_request;
        State m_state;
        ResourceState m_resource_state;
        union {
            TheBufferedFile m_buffered_file;
            GcodeSlot *m_gcode_slot;
            struct {
                TheWebRequestCallback *callback;
                alignas(CustomHandlerMaxAlign) char state[CustomHandlerMaxSize];
            } m_custom_req;
        };
        union {
            struct {
                char const *m_file_path;
                size_t m_cur_chunk_size;
            };
            struct {
                MemRef req_type;
                JsonBuilder builder;
                bool resp_body_pending;
                bool custom_waiting;
            } m_json_req;
        };
    };
    
    static GcodeSlot * find_available_gcode_slot (Context c)
    {
        auto *o = Object::self(c);
        
        for (GcodeSlot &slot : o->gcode_slots) {
            if (slot.isAvailable(c)) {
                return &slot;
            }
        }
        return nullptr;
    }
    
    class GcodeSlot : private TheConvenientStream::UserCallback {
    private:
        enum class State : uint8_t {AVAILABLE, ATTACHED, FINISHING};
        
    public:
        void init (Context c)
        {
            m_state = State::AVAILABLE;
        }
        
        void deinit (Context c)
        {
            if (m_state != State::AVAILABLE) {
                m_command_stream.deinit(c);
                m_gcode_parser.deinit(c);
            }
        }
        
        bool isAvailable (Context c)
        {
            return (m_state == State::AVAILABLE);
        }
        
        void attach (Context c, UserClientState *client)
        {
            AMBRO_ASSERT(m_state == State::AVAILABLE)
            
            m_gcode_parser.init(c);
            m_command_stream.init(c, GcodeSendBufTimeoutTicks, this, APRINTER_CB_OBJFUNC_T(&GcodeSlot::next_event_handler, this));
            m_command_stream.setPokeOverhead(c, TheHttpServer::MaxTxChunkOverhead);
            
            m_state = State::ATTACHED;
            m_client = client;
            m_buffer_pos = 0;
            m_output_pos = 0;
            
            m_client->m_request->adoptRequestBody(c);
            
            // Some browsers would fail to pass returned data to the javascript code
            // until some initial part of the response has been received. Adding this
            // header to the response works around the problem.
            // See: http://stackoverflow.com/a/26165175/1020667
            m_client->m_request->setResponseExtraHeaders(c, "X-Content-Type-Options: nosniff\r\n");
            
            m_client->m_request->adoptResponseBody(c);
            
            m_client->m_request->controlRequestBodyTimeout(c, true);
        }
        
        void detach (Context c)
        {
            AMBRO_ASSERT(m_state == State::ATTACHED)
            
            if (m_command_stream.tryCancelCommand(c)) {
                reset(c);
            } else {
                m_state = State::FINISHING;
                m_command_stream.updateSendBufEvent(c);
            }
        }
        
        void requestBufferEvent (Context c)
        {
            AMBRO_ASSERT(m_state == State::ATTACHED)
            
            m_command_stream.setNextEventIfNoCommand(c);
        }
        
        void responseBufferEvent (Context c)
        {
            AMBRO_ASSERT(m_state == State::ATTACHED)
            
            m_command_stream.updateSendBufEvent(c);
        }
        
    private:
        void reset (Context c)
        {
            AMBRO_ASSERT(m_state == OneOf(State::ATTACHED, State::FINISHING))
            
            m_command_stream.deinit(c);
            m_gcode_parser.deinit(c);
            
            m_state = State::AVAILABLE;
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
                    m_client->m_request->controlRequestBodyTimeout(c, false);
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
        
        void finish_command_impl (Context c) override
        {
            AMBRO_ASSERT(m_state == OneOf(State::ATTACHED, State::FINISHING))
            
            size_t cmd_len = m_gcode_parser.getLength(c);
            AMBRO_ASSERT(cmd_len <= m_buffer_pos)
            m_buffer_pos -= cmd_len;
            memmove(m_buffer, m_buffer + cmd_len, m_buffer_pos);
            
            if (m_state == State::ATTACHED) {
                m_client->m_request->controlRequestBodyTimeout(c, true);
            }
            
            m_command_stream.setNextEventAfterCommandFinished(c);
        }
        
        void reply_poke_impl (Context c, bool push) override
        {
            AMBRO_ASSERT(m_state == OneOf(State::ATTACHED, State::FINISHING))
            
            if (m_state == State::ATTACHED) {
                if (m_output_pos > 0) {
                    m_client->m_request->provideResponseBodyData(c, m_output_pos);
                    m_output_pos = 0;
                }
                if (push) {
                    m_client->m_request->pushResponseBody(c);
                }
            }
        }
        
        void reply_append_buffer_impl (Context c, char const *str, size_t length) override
        {
            AMBRO_ASSERT(m_state == OneOf(State::ATTACHED, State::FINISHING))
            
            if (m_state == State::ATTACHED && !m_command_stream.isSendOverrunBeingRaised(c) && length > 0) {
                auto buf_st = m_client->m_request->getResponseBodyBufferState(c);
                AMBRO_ASSERT(buf_st.length >= m_output_pos)
                if (buf_st.length - m_output_pos < length) {
                    return m_command_stream.raiseSendOverrun(c);
                }
                buf_st.data.subFrom(m_output_pos).copyIn(MemRef(str, length));
                m_output_pos += length;
            }
        }
        
        size_t get_send_buf_avail_impl (Context c) override
        {
            AMBRO_ASSERT(m_state == OneOf(State::ATTACHED, State::FINISHING))
            
            if (m_state != State::ATTACHED) {
                return (size_t)-1;
            }
            auto buf_st = m_client->m_request->getResponseBodyBufferState(c);
            AMBRO_ASSERT(buf_st.length >= m_output_pos)
            return buf_st.length - m_output_pos;
        }
        
        void commandStreamError (Context c, typename TheConvenientStream::Error error) override
        {
            AMBRO_ASSERT(m_state == OneOf(State::ATTACHED, State::FINISHING))
            
            // Ignore errors if we're detached already and waiting for command to finish.
            if (m_state != State::ATTACHED) {
                return;
            }
            
            m_command_stream.setAcceptMsg(c, false);
            
            if (error == TheConvenientStream::Error::SENDBUF_TIMEOUT) {
                ThePrinterMain::print_pgm_string(c, AMBRO_PSTR("//HttpGcodeSendBufTimeout\n"));
                m_client->m_request->assumeTimeoutAtComplete(c);
            }
            else if (error == TheConvenientStream::Error::SENDBUF_OVERRUN) {
                ThePrinterMain::print_pgm_string(c, AMBRO_PSTR("//HttpGcodeSendBufOverrun\n"));
            }
            
            return m_client->complete_request(c);
        }
        
        bool mayWaitForSendBuf (Context c, size_t length) override
        {
            AMBRO_ASSERT(m_state == OneOf(State::ATTACHED, State::FINISHING))
            
            return m_state != State::ATTACHED || (m_output_pos <= TheHttpServer::GuaranteedTxChunkSizeWithoutPoke
                   && length <= TheHttpServer::GuaranteedTxChunkSizeWithoutPoke - m_output_pos);
        }
        
    private:
        UserClientState *m_client;
        TheGcodeParser m_gcode_parser;
        TheConvenientStream m_command_stream;
        size_t m_buffer_pos;
        size_t m_output_pos;
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

}

#endif
