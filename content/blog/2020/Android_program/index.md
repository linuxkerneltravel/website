---
title: "Android中eBPF的编写基础介绍"
date: 2020-10-12T10:38:35+08:00
author: "张孝家"
keywords: ["map","bpf"]
categories : ["Android移植"]
banner : "img/blogimg/zxj0.jpg"
summary : "本篇文展主要介绍在Android中如何编写eBPF程序。内容主要涉及两个部分内核态的eBPF c程序和用户态的交互程序编写接口的介绍。"
---

## Android中eBPF的编写基础介绍

简介：Android中eBPF的编写，会涉及到三个文件的编写，内核态的eBPF c程序、用户态的交互程序和编译用的bp文件。本文主要介绍内核态和用户态程序编写常用的API接口。

### 一、内核态编写

###### 1.map的定义

介绍：map主要功能是被用作内核态和用户态进行消息交互的主要接口。通过map，我们可以在内核态中将采集到的数据保存在map空间。然后，我们可以在用户态程序将map空间的数据读取出来进行处理。除此之外，map的使用效率是极高的，原因：map空间是内核和用户空间之间共享，这也意味着我们在内核态采集的数据保存到map空间中，用户需要读取到采集的数据，不需要二次拷贝出来，可以直接从map空间中读取，效率极高。

DEFINE_BPF_MAP(the_map, TYPE, TypeOfKey, TypeOfValue, num_entries)

- the_map:map的名字
- TYPE：map使用的类型。常用类型为HASH,ARRAY等。
- TypeOfKey：map中使用key键的类型。如int，long或者自定义的任意类型。
- TypeOfValue：map中保存value值的类型。如int，long或者自定义的任意类型。
- num_entries：map中保存最大项的个数。

例子：DEFINE_BPF_MAP(kprobe_map, ARRAY, int, uint32_t, 1024);

定义一个名为kprobe_map，类型为ARRAY，key键的类型是int，value值的类型是uint32_t，map中保存的最大项的个数1024。

###### 2.map的操作

上面定义map，那么对map的常用操作有：更新，查找和删除。

在讲这些常用的操作之前，我们首先将map的定义进行展开，我们自然而然就知道如何使用了。

```c
在源码中，代码如下所示
/* type safe macro to declare a map and related accessor functions */
#define DEFINE_BPF_MAP_NO_ACCESSORS(the_map, TYPE, TypeOfKey, TypeOfValue, num_entries) \
    struct bpf_map_def SEC("maps") the_map = {                                          \
            .type = BPF_MAP_TYPE_##TYPE,                                                \
            .key_size = sizeof(TypeOfKey),                                              \
            .value_size = sizeof(TypeOfValue),                                          \
            .max_entries = (num_entries),                                               \
    };

#define DEFINE_BPF_MAP(the_map, TYPE, TypeOfKey, TypeOfValue, num_entries)              \
 DEFINE_BPF_MAP_NO_ACCESSORS(the_map, TYPE, TypeOfKey, TypeOfValue, num_entries)        \                                                                                         \
 static inline __always_inline __unused TypeOfValue* bpf_##the_map##_lookup_elem(       \
            TypeOfKey* k) {                                                             \
        return unsafe_bpf_map_lookup_elem(&the_map, k);                                 \
  };                                                                                    \
                                                                                        \
 static inline __always_inline __unused int bpf_##the_map##_update_elem(                \
         TypeOfKey* k, TypeOfValue* v, unsigned long long flags) {                      \
     return unsafe_bpf_map_update_elem(&the_map, k, v, flags);                          \
 };                                                                                     \                                                                                         \
 static inline __always_inline __unused int bpf_##the_map##_delete_elem(TypeOfKey* k) { \
     return unsafe_bpf_map_delete_elem(&the_map, k);                                    \
 };
```

现在详细的说下上面1中的定义。

```
#define DEFINE_BPF_MAP(the_map, TYPE, TypeOfKey, TypeOfValue, num_entries)              \
 DEFINE_BPF_MAP_NO_ACCESSORS(the_map, TYPE, TypeOfKey, TypeOfValue, num_entries)        \
 
#define DEFINE_BPF_MAP_NO_ACCESSORS(the_map, TYPE, TypeOfKey, TypeOfValue, num_entries) \
    struct bpf_map_def SEC("maps") the_map = {                                          \
            .type = BPF_MAP_TYPE_##TYPE,                                                \
            .key_size = sizeof(TypeOfKey),                                              \
            .value_size = sizeof(TypeOfValue),                                          \
            .max_entries = (num_entries),                                               \
    }
```

其实，当我们使用DEFINE_BPF_MAP（）进行展开的时候，部分展开是创建一个名为```struct bpf_map_def SEC("maps") the_map```的结构体，这个才是真正的定义。

DEFINE_BPF_MAP（）的展开，除了部分展开是创建结构体，还有部分展开是对定义的map的操作的方法。如下所示：

```c
 //查找
 static inline __always_inline __unused TypeOfValue* bpf_##the_map##_lookup_elem(       \
            TypeOfKey* k) {                                                             \
        return unsafe_bpf_map_lookup_elem(&the_map, k);                                 \
  };                                                                                    \
                                                                                        \
 //更新
 static inline __always_inline __unused int bpf_##the_map##_update_elem(                \
         TypeOfKey* k, TypeOfValue* v, unsigned long long flags) {                      \
     return unsafe_bpf_map_update_elem(&the_map, k, v, flags);                          \
 };                                                                                     \                                                                                         \
 //删除
 static inline __always_inline __unused int bpf_##the_map##_delete_elem(TypeOfKey* k) { \
     return unsafe_bpf_map_delete_elem(&the_map, k);                                    \
 };
```

总结：通过上面的分析，我们知道在使用DEFINE_BPF_MAP（）定义map的时候，不仅仅是创建```struct bpf_map_def SEC("maps")  the_map```的结构进行定义，而且也会创建```the_map```的map的操作方法（查询，更新和删除）。

例子：DEFINE_BPF_MAP(kprobe_map, ARRAY, int, uint32_t, 1024);

对kprobe_map的操作：

```c
前提：
    int key = 0, ret;
    uint32_t *value, init_val = 0;
```

* 查找：value = bpf_kprobe_map_lookup_elem(&key);

* 更新：ret = bpf_kprobe_map_update_elem(&key, &init_val, BPF_ANY);
* 删除：ret = bpf_kprobe_map_delete_elem(&key);

###### 3.eBPF C格式

介绍：在内核态中eBPF c编写的格式如下。

```
SEC("PROGTYPE/PROGNAME")
int PROGFUNC(..args..) {
   <body-of-code
    ... read or write to MY_MAPNAME
    ... do other things
   >
}
```

该程序会定义 `PROGFUNC` 函数。编译时，系统会将此函数放在一个区段中。该区段的名称必须采用 `PROGTYPE/PROGNAME` 格式。`PROGTYPE` 可以是以下任意一项。

| **kprobe**                  | 使用 kprobe 基础架构将 `PROGFUNC` 挂接到某个内核指令。`PROGNAME` 必须是 kprobe 目标内核函数的名称。 |
| --------------------------- | ------------------------------------------------------------ |
| **tracepoint**              | 将 `PROGFUNC` 挂接到某个跟踪点。`PROGNAME` 必须采用 `SUBSYSTEM/EVENT` 格式。例如，用于将函数附加到调度程序上下文切换事件的跟踪点区段将为 `SEC("tracepoint/sched/sched_switch")`，其中 `sched` 是跟踪子系统的名称，`sched_switch` 是跟踪事件的名称。 |
| **skfilter**                | 程序将用作网络套接字过滤器。                                 |
| **schedcls**                | 程序将用作网络流量分类器。                                   |
| **cgroupskb 和 cgroupsock** | 只要 CGroup 中的进程创建了 AF_INET 或 AF_INET6 套接字，程序就会运行。 |

例如：下面是一个完整的 C 程序，它创建了一个映射并定义了一个 `tp_sched_switch` 函数，该函数可以附加到 `sched:sched_switch trace` 事件。该程序添加了与曾在特定 CPU 上运行的最新任务 PID 相关的信息。将其命名为 `myschedtp.c`。

```c
#include <linux/bpf.h>
#include <stdbool.h>
#include <stdint.h>
#include <bpf_helpers.h>

DEFINE_BPF_MAP(cpu_pid_map, ARRAY, int, uint32_t, 1024);

struct switch_args {
    unsigned long long ignore;
    char prev_comm[16];
    int prev_pid;
    int prev_prio;
    long long prev_state;
    char next_comm[16];
    int next_pid;
    int next_prio;
};

SEC("tracepoint/sched/sched_switch")
int tp_sched_switch(struct switch_args* args) {
    int key;
    uint32_t val;

    key = bpf_get_smp_processor_id();
    val = args->next_pid;

    bpf_cpu_pid_map_update_elem(&key, &val, BPF_ANY);
    return 0;
}

char _license[] SEC("license") = "GPL";
```

**内核态编写的总结**：在内核态程序编写中，目的就是采集内核数据，将采集的数据保存下，以便用户查看。因此，内核态的程序编写就涉及到采集，和数据保存。采集数据的程序就会涉及上面使用何种PROGTYPE（如tracepoint和kprobe等），数据保存就使用map。在使用宏DEFINE_BPF_MAP（）定义map的使用，除了创建map对象，同时也创建map对象的操作方法。

注意：在内核态程序编写的最后要添加许可申明。char _license[] SEC("license") = "GPL";

### 二、用户态编写

前提介绍：Android 包含一个 eBPF 加载器和库，它可在 Android 启动时加载 eBPF 程序以扩展内核功能。在 Android 启动期间，系统会加载位于 `/system/etc/bpf/` 的所有 eBPF 程序。这些程序是 Android 构建系统根据 C 程序和 Android 源代码树中的 `Android.bp` 文件构建而成的二进制对象。构建系统将生成的对象存储在 `/system/etc/bpf`，这些对象将成为系统映像的一部分。

###### 1. sysfs 中的可用文件

在启动过程中，Android 系统会自动从 `/system/etc/bpf/` 加载所有 eBPF 对象、创建程序所需的映射，并将加载的程序及其映射固定到 BPF 文件系统。这些文件随后可用于与 eBPF 程序进一步交互或读取映射。本部分介绍了这些文件的命名规范及它们在 sysfs 中的位置。

系统会创建并固定以下文件：

* 对于加载的任何程序，假设 `PROGNAME` 是程序的名称，而 `FILENAME` 是 eBPF C 文件的名称，则 Android 加载器会创建每个程序并将其固定到 `/sys/fs/bpf/prog_FILENAME_PROGTYPE_PROGNAME`。

  例如，对于上述 `myschedtp.c` 中的 `sched_switch` 跟踪点示例，系统会创建一个程序文件并将其固定到 `/sys/fs/bpf/prog_myschedtp_tracepoint_sched_sched_switch`。

* 对于创建的任何映射，假设 `MAPNAME` 是映射的名称，而 `FILENAME` 是 eBPF C 文件的名称，则 Android 加载器会创建每个映射并将其固定到 `/sys/fs/bpf/map_FILENAME_MAPNAME`。

  例如，对于上述 `myschedtp.c` 中的 `sched_switch` 跟踪点示例，系统会创建一个映射文件并将其固定到 `/sys/fs/bpf/map_myschedtp_cpu_pid_map`。

* Android BPF 库中的 `bpf_obj_get()` 可从已固定的 `/sys/fs/bpf` 文件返回文件描述符。此文件描述符可用于进一步的操作，例如读取映射或将程序附加到跟踪点。

例如：

```c
  char *tp_prog_path = "/sys/fs/bpf/prog_myschedtp_tracepoint_sched_sched_switch";
  char *tp_map_path = "/sys/fs/bpf/map_myschedtp_cpu_pid";

  int mProgFd = bpf_obj_get(tp_prog_path);
  int mMapFd = bpf_obj_get(tp_map_path);
```

###### 2. map的操作

在Andorid中对map的操作有专门的c++编写的类方法。

```c++
template <class Key, class Value>
class BpfMap {
  public:
    BpfMap<Key, Value>() : mMapFd(-1){};
    explicit BpfMap<Key, Value>(int fd) : mMapFd(fd){};
    BpfMap<Key, Value>(bpf_map_type map_type, uint32_t max_entries, uint32_t map_flags) {
        int map_fd = createMap(map_type, sizeof(Key), sizeof(Value), max_entries, map_flags);
        if (map_fd < 0) {
            mMapFd.reset(-1);
        } else {
            mMapFd.reset(map_fd);
        }
    }

    netdutils::StatusOr<Key> getFirstKey() const {
        Key firstKey;
        if (getFirstMapKey(mMapFd, &firstKey)) {
            return netdutils::statusFromErrno(
                errno, base::StringPrintf("Get firstKey map %d failed", mMapFd.get()));
        }
        return firstKey;
    }

    netdutils::StatusOr<Key> getNextKey(const Key& key) const {
        Key nextKey;
        if (getNextMapKey(mMapFd, const_cast<Key*>(&key), &nextKey)) {
            return netdutils::statusFromErrno(
                errno, base::StringPrintf("Get next key of map %d failed", mMapFd.get()));
        }
        return nextKey;
    }

    netdutils::Status writeValue(const Key& key, const Value& value, uint64_t flags) {
        if (writeToMapEntry(mMapFd, const_cast<Key*>(&key), const_cast<Value*>(&value), flags)) {
            return netdutils::statusFromErrno(
                errno, base::StringPrintf("write to map %d failed", mMapFd.get()));
        }
        return netdutils::status::ok;
    }

    netdutils::StatusOr<Value> readValue(const Key key) const {
        Value value;
        if (findMapEntry(mMapFd, const_cast<Key*>(&key), &value)) {
            return netdutils::statusFromErrno(
                errno, base::StringPrintf("read value of map %d failed", mMapFd.get()));
        }
        return value;
    }

    netdutils::Status deleteValue(const Key& key) {
        if (deleteMapEntry(mMapFd, const_cast<Key*>(&key))) {
            return netdutils::statusFromErrno(
                errno, base::StringPrintf("delete entry from map %d failed", mMapFd.get()));
        }
        return netdutils::status::ok;
    }

    // Function that tries to get map from a pinned path.
    netdutils::Status init(const char* path);

    // Iterate through the map and handle each key retrieved based on the filter
    // without modification of map content.
    netdutils::Status iterate(
        const std::function<netdutils::Status(const Key& key, const BpfMap<Key, Value>& map)>&
            filter) const;

    // Iterate through the map and get each <key, value> pair, handle each <key,
    // value> pair based on the filter without modification of map content.
    netdutils::Status iterateWithValue(
        const std::function<netdutils::Status(const Key& key, const Value& value,
                                              const BpfMap<Key, Value>& map)>& filter) const;

    // Iterate through the map and handle each key retrieved based on the filter
    netdutils::Status iterate(
        const std::function<netdutils::Status(const Key& key, BpfMap<Key, Value>& map)>& filter);

    // Iterate through the map and get each <key, value> pair, handle each <key,
    // value> pair based on the filter.
    netdutils::Status iterateWithValue(
        const std::function<netdutils::Status(const Key& key, const Value& value,
                                              BpfMap<Key, Value>& map)>& filter);

    const base::unique_fd& getMap() const { return mMapFd; };

    // Move constructor
    void operator=(BpfMap<Key, Value>&& other) noexcept {
        mMapFd = std::move(other.mMapFd);
        other.reset();
    }

    void reset(int fd = -1) {
        mMapFd.reset(fd);
    }

    bool isValid() const { return mMapFd != -1; }

    // It is only safe to call this method if it is guaranteed that nothing will concurrently
    // iterate over the map in any process.
    netdutils::Status clear() {
        const auto deleteAllEntries = [](const Key& key, BpfMap<Key, Value>& map) {
            netdutils::Status res = map.deleteValue(key);
            if (!isOk(res) && (res.code() != ENOENT)) {
                ALOGE("Failed to delete data %s\n", strerror(res.code()));
            }
            return netdutils::status::ok;
        };
        RETURN_IF_NOT_OK(iterate(deleteAllEntries));
        return netdutils::status::ok;
    }

    netdutils::StatusOr<bool> isEmpty() const {
        auto key = this->getFirstKey();
        // Return error code ENOENT means the map is empty
        if (!isOk(key) && key.status().code() == ENOENT) return true;
        RETURN_IF_NOT_OK(key);
        return false;
    }

  private:
    base::unique_fd mMapFd;
};
```

**用户态编写总结**：用户态的程序主要是用来从map中读取到采集的数据。然后，打印出来给用户看。要读取map中的数据，首先要找到map对应的文件描述符fd，通过bpf_obj_get（）函数（找到map对应的路径文件/sys/fs/bpf/下的map），返回给本用户程序的fd。然后，通过fd和map对应的key键值，读取map中的数值。

例如：

```c++
  android::bpf::BpfMap<int, int> myMap(mMapFd);
  printf("last PID running on CPU %d is %d\n", 0, myMap.readValue(0));
```

参考文档：

* [官网：使用 eBPF 扩展内核](https://source.android.google.cn/devices/architecture/kernel/bpf)

* [用户态编写api](https://www.androidos.net.cn/android/10.0.0_r6/xref/external/bcc/src/cc/libbpf.c)

* [内核态编写api](https://www.androidos.net.cn/android/10.0.0_r6/xref/system/bpf/progs/include/bpf_helpers.h)
* [用户态map操作c++](https://www.androidos.net.cn/android/10.0.0_r6/xref/system/bpf/libbpf_android/include/bpf/BpfMap.h)