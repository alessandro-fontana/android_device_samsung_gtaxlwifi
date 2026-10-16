/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Legacy Wi-Fi HAL library for ath10k (QCA9377 SDIO), loaded by AOSP's
 * default android.hardware.wifi-service. No vendor features; it provides
 * the interface list, nl80211 virtual interfaces (soft AP on wlan1 next to
 * wlan0) and the regulatory country. wlan0 belongs to the driver and is
 * never deleted.
 */

#define LOG_TAG "WifiHalGtaxl"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <linux/nl80211.h>
#include <netlink/genl/ctrl.h>
#include <netlink/errno.h>
#include <netlink/genl/genl.h>
#include <netlink/msg.h>

#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include <log/log.h>

#include "hardware_legacy/wifi_hal.h"

namespace {

constexpr char kPrimaryIface[] = "wlan0";
constexpr char kSysNet[] = "/sys/class/net";

}  // namespace

struct wifi_interface_info {
    char name[IFNAMSIZ];
};

struct wifi_info {
    std::mutex lock;
    int stop_pipe[2] = {-1, -1};
    // One object per name, never freed: the service keeps handles across
    // rescans, and a handle must not outlive the memory it points to.
    std::map<std::string, wifi_interface_info*> pool;
    std::vector<wifi_interface_handle> handles;
    std::set<std::string> created;  // interfaces this library has added
};

namespace {

wifi_info* asInfo(wifi_handle h) {
    return reinterpret_cast<wifi_info*>(h);
}

bool readSysfs(const std::string& path, std::string* out) {
    FILE* f = fopen(path.c_str(), "re");
    if (!f) return false;
    char buf[64] = {};
    bool ok = fgets(buf, sizeof(buf), f) != nullptr;
    fclose(f);
    if (!ok) return false;
    out->assign(buf);
    while (!out->empty() && (out->back() == '\n' || out->back() == '\r')) out->pop_back();
    return true;
}

/* Index of the wiphy behind an interface, -1 if it is not a wireless one. */
int wiphyIndex(const char* ifname) {
    std::string value;
    if (!readSysfs(std::string(kSysNet) + "/" + ifname + "/phy80211/index", &value)) return -1;
    return atoi(value.c_str());
}

/*
 * Check sysfs, not if_nametoindex(): an ioctl on a missing name makes the
 * kernel try to load netdev-<name>, an audited sys_module denial.
 */
bool ifaceExists(const char* ifname) {
    return access((std::string(kSysNet) + "/" + ifname).c_str(), F_OK) == 0;
}

bool readMac(const char* ifname, uint8_t mac[6]) {
    std::string value;
    if (!readSysfs(std::string(kSysNet) + "/" + ifname + "/address", &value)) return false;
    unsigned int b[6];
    if (sscanf(value.c_str(), "%x:%x:%x:%x:%x:%x", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) != 6)
        return false;
    for (int i = 0; i < 6; i++) mac[i] = b[i];
    return true;
}

/* One nl80211 request and its ack, on a socket of its own. */
class Nl80211 {
  public:
    Nl80211() {
        sock_ = nl_socket_alloc();
        if (!sock_) return;
        if (genl_connect(sock_) < 0) return;
        family_ = genl_ctrl_resolve(sock_, "nl80211");
    }
    ~Nl80211() {
        if (sock_) nl_socket_free(sock_);
    }
    bool ok() const { return sock_ && family_ >= 0; }

    nl_msg* message(uint8_t cmd) {
        nl_msg* msg = nlmsg_alloc();
        if (msg && !genlmsg_put(msg, NL_AUTO_PORT, NL_AUTO_SEQ, family_, 0, 0, cmd, 0)) {
            nlmsg_free(msg);
            return nullptr;
        }
        return msg;
    }

    /* Sends and waits for the ack; returns 0 or a negative libnl error. */
    int send(nl_msg* msg) {
        int err = nl_send_auto(sock_, msg);
        nlmsg_free(msg);
        if (err < 0) return err;
        err = nl_wait_for_ack(sock_);
        return err < 0 ? err : 0;
    }

  private:
    nl_sock* sock_ = nullptr;
    int family_ = -1;
};

uint32_t toNl80211Type(wifi_interface_type type) {
    switch (type) {
        case WIFI_INTERFACE_TYPE_AP:
            return NL80211_IFTYPE_AP;
        case WIFI_INTERFACE_TYPE_STA:
            return NL80211_IFTYPE_STATION;
        default:
            return NL80211_IFTYPE_UNSPECIFIED;
    }
}

void refreshIfaces(wifi_info* info) {
    info->handles.clear();

    DIR* dir = opendir(kSysNet);
    if (!dir) {
        ALOGE("cannot open %s: %s", kSysNet, strerror(errno));
        return;
    }
    while (dirent* entry = readdir(dir)) {
        if (entry->d_name[0] == '.') continue;
        if (wiphyIndex(entry->d_name) < 0) continue;
        auto& iface = info->pool[entry->d_name];
        if (!iface) {
            iface = new wifi_interface_info();
            strlcpy(iface->name, entry->d_name, sizeof(iface->name));
        }
        info->handles.push_back(iface);
    }
    closedir(dir);
}

}  // namespace

wifi_error wifi_initialize(wifi_handle* handle) {
    if (!handle) return WIFI_ERROR_INVALID_ARGS;
    /*
     * The service may stop and restart the HAL: keep one state for the whole
     * process and only renew what a stop consumes.
     */
    static wifi_info* sInfo = new wifi_info();
    std::lock_guard<std::mutex> guard(sInfo->lock);
    for (int& fd : sInfo->stop_pipe) {
        if (fd >= 0) close(fd);
        fd = -1;
    }
    if (pipe2(sInfo->stop_pipe, O_CLOEXEC) < 0) {
        ALOGE("pipe2: %s", strerror(errno));
        return WIFI_ERROR_UNKNOWN;
    }
    refreshIfaces(sInfo);
    *handle = reinterpret_cast<wifi_handle>(sInfo);
    ALOGI("initialized, %zu wireless interface(s)", sInfo->handles.size());
    return WIFI_SUCCESS;
}

/*
 * Runs until wifi_cleanup(): the service treats a loop that returns on its
 * own as fatal. There are no events to handle.
 */
void wifi_event_loop(wifi_handle handle) {
    wifi_info* info = asInfo(handle);
    if (!info) return;
    pollfd pfd = {info->stop_pipe[0], POLLIN, 0};
    while (poll(&pfd, 1, -1) < 0 && errno == EINTR) {
    }
    ALOGI("event loop terminated");
}

/*
 * Called with the service's global lock held, and the service expects the
 * handler on this same thread: ask the loop to end and report at once.
 */
void wifi_cleanup(wifi_handle handle, wifi_cleaned_up_handler handler) {
    wifi_info* info = asInfo(handle);
    if (!info) return;
    char c = 1;
    if (write(info->stop_pipe[1], &c, 1) != 1) ALOGE("cannot wake the event loop: %s", strerror(errno));
    if (handler) handler(handle);
}

wifi_error wifi_get_ifaces(wifi_handle handle, int* num, wifi_interface_handle** ifaces) {
    wifi_info* info = asInfo(handle);
    if (!info || !num || !ifaces) return WIFI_ERROR_INVALID_ARGS;
    std::lock_guard<std::mutex> guard(info->lock);
    refreshIfaces(info);
    *num = info->handles.size();
    *ifaces = info->handles.data();
    return WIFI_SUCCESS;
}

wifi_error wifi_get_iface_name(wifi_interface_handle iface, char* name, size_t size) {
    if (!iface || !name || !size) return WIFI_ERROR_INVALID_ARGS;
    strlcpy(name, iface->name, size);
    return WIFI_SUCCESS;
}

wifi_error wifi_virtual_interface_create(wifi_handle handle, const char* ifname,
                                         wifi_interface_type type) {
    wifi_info* info = asInfo(handle);
    if (!info || !ifname) return WIFI_ERROR_INVALID_ARGS;

    // An interface that already exists is fine: the service asks for wlan0 too.
    if (ifaceExists(ifname)) return WIFI_SUCCESS;

    uint32_t nltype = toNl80211Type(type);
    int wiphy = wiphyIndex(kPrimaryIface);
    uint8_t mac[6];
    if (nltype == NL80211_IFTYPE_UNSPECIFIED || wiphy < 0 || !readMac(kPrimaryIface, mac)) {
        ALOGE("cannot create %s (type %d, wiphy %d)", ifname, type, wiphy);
        return WIFI_ERROR_NOT_SUPPORTED;
    }

    /*
     * mac80211 refuses a station and an AP with the same address
     * (ieee80211_check_concurrent_iface): derive a locally administered one
     * from wlan0's.
     */
    uint8_t salt = 0;
    for (const char* p = ifname; *p; p++) salt = salt * 31 + *p;
    mac[0] |= 0x02;
    mac[5] ^= salt ? salt : 0x5a;

    Nl80211 nl;
    nl_msg* msg = nl.ok() ? nl.message(NL80211_CMD_NEW_INTERFACE) : nullptr;
    if (!msg) {
        ALOGE("nl80211 not available");
        return WIFI_ERROR_UNKNOWN;
    }
    nla_put_u32(msg, NL80211_ATTR_WIPHY, wiphy);
    nla_put_string(msg, NL80211_ATTR_IFNAME, ifname);
    nla_put_u32(msg, NL80211_ATTR_IFTYPE, nltype);
    nla_put(msg, NL80211_ATTR_MAC, sizeof(mac), mac);
    int err = nl.send(msg);
    if (err) {
        ALOGE("NEW_INTERFACE %s type %u: %s", ifname, nltype, nl_geterror(err));
        return WIFI_ERROR_UNKNOWN;
    }

    std::lock_guard<std::mutex> guard(info->lock);
    info->created.insert(ifname);
    ALOGI("created %s (nl80211 type %u) on phy%d", ifname, nltype, wiphy);
    return WIFI_SUCCESS;
}

wifi_error wifi_virtual_interface_delete(wifi_handle handle, const char* ifname) {
    wifi_info* info = asInfo(handle);
    if (!info || !ifname) return WIFI_ERROR_INVALID_ARGS;
    {
        std::lock_guard<std::mutex> guard(info->lock);
        // Only what this library added: wlan0 is the driver's.
        if (!info->created.count(ifname)) return WIFI_SUCCESS;
    }
    unsigned int index = ifaceExists(ifname) ? if_nametoindex(ifname) : 0;
    if (index) {
        Nl80211 nl;
        nl_msg* msg = nl.ok() ? nl.message(NL80211_CMD_DEL_INTERFACE) : nullptr;
        if (!msg) return WIFI_ERROR_UNKNOWN;
        nla_put_u32(msg, NL80211_ATTR_IFINDEX, index);
        int err = nl.send(msg);
        if (err) {
            ALOGE("DEL_INTERFACE %s: %s", ifname, nl_geterror(err));
            return WIFI_ERROR_UNKNOWN;
        }
    }
    std::lock_guard<std::mutex> guard(info->lock);
    info->created.erase(ifname);
    ALOGI("deleted %s", ifname);
    return WIFI_SUCCESS;
}

wifi_error wifi_set_country_code(wifi_interface_handle iface, const char* code) {
    if (!code || strlen(code) < 2) return WIFI_ERROR_INVALID_ARGS;
    char alpha2[3] = {code[0], code[1], 0};
    Nl80211 nl;
    nl_msg* msg = nl.ok() ? nl.message(NL80211_CMD_REQ_SET_REG) : nullptr;
    if (!msg) return WIFI_ERROR_UNKNOWN;
    nla_put_string(msg, NL80211_ATTR_REG_ALPHA2, alpha2);
    int err = nl.send(msg);
    if (err) {
        ALOGE("REQ_SET_REG %s: %s", alpha2, nl_geterror(err));
        return WIFI_ERROR_UNKNOWN;
    }
    return WIFI_SUCCESS;
}

wifi_error wifi_get_supported_feature_set(wifi_interface_handle iface, feature_set* set) {
    if (!set) return WIFI_ERROR_INVALID_ARGS;
    *set = WIFI_FEATURE_INFRA | WIFI_FEATURE_INFRA_5G | WIFI_FEATURE_SOFT_AP;
    return WIFI_SUCCESS;
}

wifi_error init_wifi_vendor_hal_func_table(wifi_hal_fn* fn) {
    if (!fn) return WIFI_ERROR_INVALID_ARGS;
    fn->wifi_initialize = wifi_initialize;
    fn->wifi_event_loop = wifi_event_loop;
    fn->wifi_cleanup = wifi_cleanup;
    fn->wifi_get_ifaces = wifi_get_ifaces;
    fn->wifi_get_iface_name = wifi_get_iface_name;
    fn->wifi_virtual_interface_create = wifi_virtual_interface_create;
    fn->wifi_virtual_interface_delete = wifi_virtual_interface_delete;
    fn->wifi_set_country_code = wifi_set_country_code;
    fn->wifi_get_supported_feature_set = wifi_get_supported_feature_set;
    return WIFI_SUCCESS;
}
