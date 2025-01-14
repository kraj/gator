/* Copyright (C) 2010-2021 by Arm Limited. All rights reserved. */

#pragma once

#include <memory>

#include <semaphore.h>
class Source;

namespace mali_userspace {
    class MaliHwCntrDriver;
    std::unique_ptr<Source> createMaliHwCntrSource(sem_t & senderSem, MaliHwCntrDriver & driver);
}
