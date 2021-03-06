/* -*- Mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 *     Copyright 2017 Couchbase, Inc.
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */

#include <daemon/mcbp.h>
#include "executors.h"

void process_hello_packet_executor(McbpConnection* c, void* packet) {
    auto* req = reinterpret_cast<protocol_binary_request_hello*>(packet);
    std::string log_buffer;
    log_buffer.reserve(512);
    log_buffer.append("HELO ");

    const cb::const_char_buffer key{
        reinterpret_cast<const char*>(req->bytes + sizeof(req->bytes)),
        ntohs(req->message.header.request.keylen)};

    const cb::sized_buffer<const uint16_t> input{
        reinterpret_cast<const uint16_t*>(key.data() + key.size()),
        (ntohl(req->message.header.request.bodylen) - key.size()) / 2};

    std::vector<uint16_t> out;
    bool tcpdelay_handled = false;

    /*
     * Disable all features the hello packet may enable, so that
     * the client can toggle features on/off during a connection
     */
    c->disableAllDatatypes();
    c->setSupportsMutationExtras(false);
    c->setXerrorSupport(false);
    c->setCollectionsSupported(false);
    c->setDuplexSupported(false);

    if (!key.empty()) {
        log_buffer.append("[");
        if (key.size() > 256) {
            log_buffer.append(key.data(), 256);
            log_buffer.append("...");
        } else {
            log_buffer.append(key.data(), key.size());
        }
        log_buffer.append("] ");
    }

    for (const auto& value : input) {
        bool added = false;
        const uint16_t in = ntohs(value);
        const auto feature = cb::mcbp::Feature(in);

        switch (feature) {
        case cb::mcbp::Feature::Invalid:
        case cb::mcbp::Feature::TLS:
            /* Not implemented */
            break;
        case cb::mcbp::Feature::TCPNODELAY:
        case cb::mcbp::Feature::TCPDELAY:
            if (!tcpdelay_handled) {
                c->setTcpNoDelay(feature == cb::mcbp::Feature::TCPNODELAY);
                tcpdelay_handled = true;
                added = true;
            }
            break;

        case cb::mcbp::Feature::MUTATION_SEQNO:
            if (!c->isSupportsMutationExtras()) {
                c->setSupportsMutationExtras(true);
                added = true;
            }
            break;
        case cb::mcbp::Feature::XATTR:
            if ((Datatype::isSupported(cb::mcbp::Feature::XATTR) ||
                 c->isInternal()) &&
                !c->isXattrEnabled()) {
                c->enableDatatype(cb::mcbp::Feature::XATTR);
                added = true;
            }
            break;
        case cb::mcbp::Feature::JSON:
            if (Datatype::isSupported(cb::mcbp::Feature::JSON) &&
                !c->isJsonEnabled()) {
                c->enableDatatype(cb::mcbp::Feature::JSON);
                added = true;
            }
            break;
        case cb::mcbp::Feature::SNAPPY:
            if (Datatype::isSupported(cb::mcbp::Feature::SNAPPY) &&
                !c->isSnappyEnabled()) {
                c->enableDatatype(cb::mcbp::Feature::SNAPPY);
                added = true;
            }
            break;
        case cb::mcbp::Feature::XERROR:
            if (!c->isXerrorSupport()) {
                c->setXerrorSupport(true);
                added = true;
            }
            break;
        case cb::mcbp::Feature::SELECT_BUCKET:
            // The select bucket is only informative ;-)
            added = true;
            break;
        case cb::mcbp::Feature::COLLECTIONS:
            if (!c->isCollectionsSupported()) {
                c->setCollectionsSupported(true);
                added = true;
            }
            break;
        case cb::mcbp::Feature::Duplex:
            if (!c->isDuplexSupported()) {
                c->setDuplexSupported(true);
                added = true;
            }
            break;
        }

        if (added) {
            out.push_back(value);
            log_buffer.append(to_string(feature));
            log_buffer.append(", ");
        }
    }

    if (out.empty()) {
        mcbp_write_packet(c, PROTOCOL_BINARY_RESPONSE_SUCCESS);
    } else {
        mcbp_response_handler(nullptr, 0, nullptr, 0,
                              out.data(),
                              uint32_t(2 * out.size()),
                              PROTOCOL_BINARY_RAW_BYTES,
                              PROTOCOL_BINARY_RESPONSE_SUCCESS,
                              0, c->getCookie());
        mcbp_write_and_free(c, &c->getDynamicBuffer());
    }

    // Trim off the trailing whitespace (and potentially comma)
    log_buffer.resize(log_buffer.size() - 1);
    if (log_buffer.back() == ',') {
        log_buffer.resize(log_buffer.size() - 1);
    }

    LOG_NOTICE(c, "%u: %s %s", c->getId(), log_buffer.c_str(),
               c->getDescription().c_str());
}
