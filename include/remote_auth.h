/* Copyright (C) 2026 Ricardo Guzman - CA2RXU
 *
 * This file is part of LoRa APRS iGate.
 *
 * LoRa APRS iGate is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef REMOTE_AUTH_H_
#define REMOTE_AUTH_H_

#include <Arduino.h>

namespace REMOTE_AUTH {

    enum class Status {
        NotEnvelope,
        Disabled,
        Malformed,
        WrongController,
        Replay,
        BadTag,
        StorageError,
        Accepted,
    };

    struct Result {
        Status status = Status::NotEnvelope;
        String command;
        uint64_t counter = 0;
    };

    bool configured();
    uint64_t acceptedCounter();
    bool setSecret(const String& base64UrlSecret);
    bool clearSecret();
    Result verifyAndConsume(const String& envelope, const String& controller,
                            const String& expectedController, const String& target);
    const char* statusText(Status status);

}

#endif
