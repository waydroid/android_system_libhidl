/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <hidl/HidlPassthroughSupport.h>

#include <hidl/HidlTransportUtils.h>
#include <hidl/Static.h>

using ::android::hidl::base::V1_0::IBase;

namespace android {
namespace hardware {
namespace details {

sp<IBase> wrapPassthroughInternal(sp<IBase> iface) {
    if (iface == nullptr || iface->isRemote()) {
        // doesn't know how to handle it.
        return iface;
    }
    std::string myDescriptor = getDescriptor(iface.get());
    if (myDescriptor.empty()) {
        // interfaceDescriptor fails
        return nullptr;
    }
    auto func = getBsConstructorMap().get(myDescriptor, nullptr);
    if (!func) {
        func = gBsConstructorMap.get(myDescriptor, nullptr);
        if (!func) {
            return nullptr;
        }
    }

    sp<IBase> base = func(static_cast<void*>(iface.get()));

    // To ensure this is an instance of IType, we would normally
    // call castFrom, but gBsConstructorMap guarantees that its
    // result is of the appropriate type (not necessaryly BsType,
    // but definitely a child of IType).
    return base;
}

}  // namespace details
}  // namespace hardware
}  // namespace android
