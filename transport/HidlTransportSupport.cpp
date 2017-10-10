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
#include <hidl/HidlTransportSupport.h>

#include <android/hidl/manager/1.0/IServiceManager.h>
#include <hidl/HidlBinderSupport.h>
#include <hidl/ServiceManagement.h>

namespace android {
namespace hardware {

void configureRpcThreadpool(size_t maxThreads, bool callerWillJoin) {
    // TODO(b/32756130) this should be transport-dependent
    configureBinderRpcThreadpool(maxThreads, callerWillJoin);
}
void joinRpcThreadpool() {
    // TODO(b/32756130) this should be transport-dependent
    joinBinderRpcThreadpool();
}

bool setMinSchedulerPolicy(const sp<::android::hidl::base::V1_0::IBase>& service,
                           int policy, int priority) {
    if (service->isRemote()) {
        ALOGE("Can't set scheduler policy on remote service.");
        return false;
    }

    if (policy != SCHED_NORMAL && policy != SCHED_FIFO && policy != SCHED_RR) {
        ALOGE("Invalid scheduler policy %d", policy);
        return false;
    }

    if (policy == SCHED_NORMAL && (priority < -20 || priority > 19)) {
        ALOGE("Invalid priority for SCHED_NORMAL: %d", priority);
        return false;
    } else if (priority < 1 || priority > 99) {
        ALOGE("Invalid priority for real-time policy: %d", priority);
        return false;
    }

    details::gServicePrioMap.set(service, { policy, priority });

    return true;
}

namespace details {

sp<::android::hidl::base::V1_0::IBase> getRawServiceInternal(const std::string& descriptor,
                                                             const std::string& instance,
                                                             bool retry, bool getStub) {
    using Transport = ::android::hidl::manager::V1_0::IServiceManager::Transport;
    using ::android::hidl::base::V1_0::IBase;
    using ::android::hidl::manager::V1_0::IServiceManager;

    const sp<IServiceManager> sm = defaultServiceManager();
    if (sm == nullptr) {
        ALOGE("getService: defaultServiceManager() is null");
        return nullptr;
    }

    Return<Transport> transportRet = sm->getTransport(descriptor, instance);

    if (!transportRet.isOk()) {
        ALOGE("getService: defaultServiceManager()->getTransport returns %s",
              transportRet.description().c_str());
        return nullptr;
    }
    Transport transport = transportRet;
    const bool vintfHwbinder = (transport == Transport::HWBINDER);
    const bool vintfPassthru = (transport == Transport::PASSTHROUGH);

#ifdef LIBHIDL_TARGET_TREBLE

#ifdef LIBHIDL_TARGET_DEBUGGABLE
    const char* env = std::getenv("TREBLE_TESTING_OVERRIDE");
    const bool trebleTestingOverride = env && !strcmp(env, "true");
    const bool vintfLegacy = (transport == Transport::EMPTY) && trebleTestingOverride;
#else   // LIBHIDL_TARGET_TREBLE but not LIBHIDL_TARGET_DEBUGGABLE
    const bool trebleTestingOverride = false;
    const bool vintfLegacy = false;
#endif  // LIBHIDL_TARGET_DEBUGGABLE

#else   // not LIBHIDL_TARGET_TREBLE
    const char* env = std::getenv("TREBLE_TESTING_OVERRIDE");
    const bool trebleTestingOverride = env && !strcmp(env, "true");
    const bool vintfLegacy = (transport == Transport::EMPTY);
#endif  // LIBHIDL_TARGET_TREBLE

    for (int tries = 0;
         !getStub && (vintfHwbinder || (vintfLegacy && tries == 0)) && (retry || tries < 1);
         tries++) {
        if (tries > 1) {
            ALOGI("getService: Will do try %d for %s/%s in 1s...", tries, descriptor.c_str(),
                  instance.c_str());
            sleep(1);  // TODO(b/67425500): remove and update waitForHwService function
        }
        if (vintfHwbinder && tries > 0) {
            waitForHwService(descriptor, instance);
        }
        Return<sp<IBase>> ret = sm->get(descriptor, instance);
        if (!ret.isOk()) {
            ALOGE("getService: defaultServiceManager()->get returns %s for %s/%s.",
                  ret.description().c_str(), descriptor.c_str(), instance.c_str());
            break;
        }
        sp<IBase> base = ret;
        if (base == nullptr) {
            if (tries > 0) {
                ALOGW("getService: found unexpected null hwbinder interface for %s/%s.",
                      descriptor.c_str(), instance.c_str());
            }
            continue;
        }

        Return<bool> canCastRet =
            details::canCastInterface(base.get(), descriptor.c_str(), true /* emitError */);

        if (!canCastRet.isOk()) {
            if (canCastRet.isDeadObject()) {
                ALOGW("getService: found dead hwbinder service for %s/%s.", descriptor.c_str(),
                      instance.c_str());
                continue;
            }
            // TODO(b/67425500): breaks getService == nullptr => hal available assumptions if the
            // service has a transaction failure (one example of this is if the service's binder
            // buffer is full). If this isn't here, you get an infinite loop when you don't have
            // permission to talk to a service.
            ALOGW("getService: unable to call into hwbinder service for %s/%s.", descriptor.c_str(),
                  instance.c_str());
            break;
        }

        if (!canCastRet) {
            ALOGW("getService: received incompatible service (bug in hwservicemanager?) for %s/%s.",
                  descriptor.c_str(), instance.c_str());
            break;
        }

        return base;  // still needs to be wrapped by Bp class.
    }

    if (getStub || vintfPassthru || vintfLegacy) {
        const sp<IServiceManager> pm = getPassthroughServiceManager();
        if (pm != nullptr) {
            sp<IBase> base = pm->get(descriptor, instance).withDefault(nullptr);
            if (!getStub || trebleTestingOverride) {
                base = wrapPassthrough(base);
            }
            return base;
        }
    }

    return nullptr;
}

}  // namespace details
}  // namespace hardware
}  // namespace android
