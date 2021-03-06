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

#include "sasl_refresh_command_context.h"

#include <daemon/mcbp.h>

ENGINE_ERROR_CODE SaslRefreshCommandContext::refresh() {
    state = State::Done;
    return refresh_cbsasl(&connection);
}

void SaslRefreshCommandContext::done() {
    mcbp_write_packet(&connection, PROTOCOL_BINARY_RESPONSE_SUCCESS);
}
