/*
 * Copyright (c) 2016 Ambroz Bizjak
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

#ifndef APRINTER_WEB_API_FILES_MODULE_H
#define APRINTER_WEB_API_FILES_MODULE_H

#include <stddef.h>
#include <stdint.h>

#include <aprinter/meta/AliasStruct.h>
#include <aprinter/meta/TypeListUtils.h>
#include <aprinter/base/Callback.h>
#include <aprinter/base/Assert.h>
#include <aprinter/fs/DirLister.h>
#include <aprinter/printer/ServiceList.h>
#include <aprinter/printer/utils/WebRequest.h>
#include <aprinter/printer/utils/ModuleUtils.h>

#include <aprinter/BeginNamespace.h>

template <typename ModuleArg>
class WebApiFilesModule {
    APRINTER_UNPACK_MODULE_ARG(ModuleArg)
    
private:
    using TheFsAccess = typename ThePrinterMain::template GetFsAccess<>;
    using TheDirLister = DirLister<Context, TheFsAccess>;
    using FsEntry   = typename TheFsAccess::TheFileSystem::FsEntry;
    using EntryType = typename TheFsAccess::TheFileSystem::EntryType;
    
public:
    template <typename WebApiConfig>
    struct WebApi {
        static_assert(WebApiConfig::JsonBufferSize >= TheFsAccess::TheFileSystem::MaxFileNameSize + 6, "");
        
        static bool handle_web_request (Context c, MemRef req_type, WebRequest<Context> *request)
        {
            if (req_type.equalTo("files")) {
                MemRef dir_path;
                if (!request->getParam(c, "dir", &dir_path)) {
                    return request->badParams(c);
                }
                bool flag_dirs = false;
                MemRef flag_dirs_param;
                if (request->getParam(c, "flagDirs", &flag_dirs_param)) {
                    flag_dirs = flag_dirs_param.equalTo("1");
                }
                return request->template acceptRequest<FilesRequest>(c, dir_path.ptr, flag_dirs);
            }
            return true;
        }
        
        class FilesRequest : public WebRequestHandler<Context, FilesRequest> {
        private:
            enum class State : uint8_t {STATE_OPEN, STATE_WAITBUF_ENTRY, STATE_REQUEST_ENTRY};
            
        public:
            void init (Context c, char const *dir_path, bool flag_dirs)
            {
                m_dir_path = dir_path;
                m_flag_dirs = flag_dirs;
                m_state = State::STATE_OPEN;
                m_dirlister.init(c, APRINTER_CB_OBJFUNC_T(&FilesRequest::dirlister_handler, this));
                m_dirlister.startOpen(c, dir_path, false, WebApiConfig::UploadBasePath());
            }
            
            void deinit (Context c)
            {
                m_dirlister.deinit(c);
            }
            
            void jsonBufferAvailable (Context c)
            {
                AMBRO_ASSERT(m_state == State::STATE_WAITBUF_ENTRY)
                
                m_state = State::STATE_REQUEST_ENTRY;
                m_dirlister.requestEntry(c);
            }
            
            void dirlister_handler (Context c, typename TheDirLister::Error error, char const *name, FsEntry entry)
            {
                switch (m_state) {
                    case State::STATE_OPEN: {
                        if (error != TheDirLister::Error::NO_ERROR) {
                            auto status = (error == TheDirLister::Error::NOT_FOUND) ? HttpStatusCodes::NotFound() : HttpStatusCodes::InternalServerError();
                            return this->completeHandling(c, status);
                        }
                        
                        JsonBuilder *json = this->startJson(c);
                        json->startObject();
                        json->addSafeKeyVal("dir", JsonString{m_dir_path});
                        json->addKeyArray(JsonSafeString{"files"});
                        if (!this->endJson(c)) {
                            return this->completeHandling(c, HttpStatusCodes::InternalServerError());
                        }
                        
                        m_state = State::STATE_WAITBUF_ENTRY;
                        this->waitForJsonBuffer(c);
                    } break;
                    
                    case State::STATE_REQUEST_ENTRY: {
                        if (error != TheDirLister::Error::NO_ERROR) {
                            return this->completeHandling(c);
                        }
                        
                        if (name && name[0] == '.') {
                            m_dirlister.requestEntry(c);
                            return;
                        }
                        
                        JsonBuilder *json = this->startJson(c);
                        
                        if (name) {
                            json->beginString();
                            if (m_flag_dirs && entry.getType() == EntryType::DIR_TYPE) {
                                json->addStringChar('*');
                            }
                            json->addStringMem(name);
                            json->endString();
                        } else {
                            json->endArray();
                            json->endObject();
                        }
                        
                        if (!this->endJson(c) || !name) {
                            return this->completeHandling(c);
                        }
                        
                        m_state = State::STATE_WAITBUF_ENTRY;
                        this->waitForJsonBuffer(c);
                    } break;
                    
                    default: AMBRO_ASSERT(false);
                }
            }
            
        private:
            TheDirLister m_dirlister;
            char const *m_dir_path;
            bool m_flag_dirs;
            State m_state;
        };
        
        using WebApiRequestHandlers = MakeTypeList<FilesRequest>;
    };
    
public:
    struct Object {};
};

struct WebApiFilesModuleService {
    APRINTER_MODULE_TEMPLATE(WebApiFilesModuleService, WebApiFilesModule)
    using ProvidedServices = MakeTypeList<ServiceDefinition<ServiceList::WebApiHandlerService>>;
};

#include <aprinter/EndNamespace.h>

#endif
