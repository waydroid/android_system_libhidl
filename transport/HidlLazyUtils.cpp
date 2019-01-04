/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include <hidl/HidlLazyUtils.h>
#include <hidl/HidlTransportSupport.h>

#include <android-base/logging.h>

#include <android/hidl/manager/1.2/IClientCallback.h>
#include <android/hidl/manager/1.2/IServiceManager.h>

namespace android {
namespace hardware {
namespace details {

class ClientCounterCallback : public ::android::hidl::manager::V1_2::IClientCallback {
   public:
    ClientCounterCallback() : mNumConnectedServices(0), mNumRegisteredServices(0) {}

    void incServiceCounter();

   protected:
    Return<void> onClients(const sp<::android::hidl::base::V1_0::IBase>& service,
                           bool clients) override;

   private:
    /**
     * Counter of the number of services that currently have at least one client.
     */
    size_t mNumConnectedServices;

    /**
     * Number of services that have been registered.
     */
    size_t mNumRegisteredServices;
};

class LazyServiceRegistrarImpl {
   public:
    LazyServiceRegistrarImpl() : mClientCallback(new ClientCounterCallback) {}

    status_t registerService(const sp<::android::hidl::base::V1_0::IBase>& service,
                             const std::string& name);

   private:
    sp<ClientCounterCallback> mClientCallback;
};

void ClientCounterCallback::incServiceCounter() {
    mNumRegisteredServices++;
}

/**
 * onClients is oneway, so no need to worry about multi-threading. Note that this means multiple
 * invocations could occur on different threads however.
 */
Return<void> ClientCounterCallback::onClients(const sp<::android::hidl::base::V1_0::IBase>& service,
                                              bool clients) {
    if (clients) {
        LOG(INFO) << "HAL " << service->descriptor << " connected.";
        mNumConnectedServices++;
    } else {
        LOG(INFO) << "HAL " << service->descriptor << " disconnected.";
        mNumConnectedServices--;
    }
    LOG(INFO) << "HAL has " << mNumConnectedServices << " (of " << mNumRegisteredServices
              << " available) clients in use.";

    if (mNumConnectedServices == 0) {
        LOG(INFO) << "Exiting HAL. No clients in use for any service in process.";
        exit(EXIT_SUCCESS);
    }
    return Status::ok();
}

status_t LazyServiceRegistrarImpl::registerService(
    const sp<::android::hidl::base::V1_0::IBase>& service, const std::string& name) {
    static auto manager = hardware::defaultServiceManager1_2();
    LOG(INFO) << "Registering HAL: " << service->descriptor << " with name: " << name;
    status_t res = android::hardware::details::registerAsServiceInternal(service, name);
    if (res == android::OK) {
        mClientCallback->incServiceCounter();
        bool ret = manager->registerClientCallback(service, mClientCallback);
        if (!ret) {
            res = android::INVALID_OPERATION;
            LOG(ERROR) << "Failed to add client callback";
        }
    } else {
        LOG(ERROR) << "Failed to register as service";
    }
    return res;
}

}  // namespace details

LazyServiceRegistrar::LazyServiceRegistrar() {
    mImpl = std::make_shared<details::LazyServiceRegistrarImpl>();
}

status_t LazyServiceRegistrar::registerService(
    const sp<::android::hidl::base::V1_0::IBase>& service, const std::string& name) {
    return mImpl->registerService(service, name);
}

}  // namespace hardware
}  // namespace android
