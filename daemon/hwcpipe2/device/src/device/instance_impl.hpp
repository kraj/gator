/*
 * Copyright (c) 2022 ARM Limited.
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include "ioctl/kbase_pre_r21/types.hpp"

#include <device/constants.hpp>
#include <device/hwcnt/backend_type.hpp>
#include <device/hwcnt/block_extents.hpp>
#include <device/hwcnt/block_metadata.hpp>
#include <device/hwcnt/sampler/kinstr_prfcnt/construct_block_extents.hpp>
#include <device/hwcnt/sampler/kinstr_prfcnt/enum_info.hpp>
#include <device/hwcnt/sampler/vinstr/construct_block_extents.hpp>
#include <device/instance.hpp>
#include <device/ioctl/kbase/commands.hpp>
#include <device/ioctl/kbase/types.hpp>
#include <device/ioctl/kbase_pre_r21/commands.hpp>
#include <device/ioctl/kbase_pre_r21/types.hpp>
#include <device/ioctl/pointer64.hpp>
#include <device/kbase_version.hpp>
#include <device/product_id.hpp>

#include <array>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <system_error>
#include <vector>

namespace hwcpipe {
namespace device {

using properties = std::vector<unsigned char>;

namespace detail {

static uint64_t get_warp_width(uint64_t raw_gpu_id, std::error_code &ec) {
    static constexpr product_id product_id_g31{7, 3};
    static constexpr product_id product_id_g51{7, 0};
    static constexpr product_id product_id_g52{7, 2};
    static constexpr product_id product_id_g57{9, 1};
    static constexpr product_id product_id_g57_2{9, 3};
    static constexpr product_id product_id_g68{9, 4};
    static constexpr product_id product_id_g71{6, 0};
    static constexpr product_id product_id_g72{6, 1};
    static constexpr product_id product_id_g76{7, 1};
    static constexpr product_id product_id_g77{9, 0};
    static constexpr product_id product_id_g78{9, 2};
    static constexpr product_id product_id_g310{10, 4};
    static constexpr product_id product_id_g510{10, 3};
    static constexpr product_id product_id_g610{10, 7};
    static constexpr product_id product_id_g710{10, 2};
    static constexpr product_id product_id_gtux{11, 2};
    static constexpr product_id product_id_gtux_2{11, 3};

    const product_id pid(raw_gpu_id);

    /* Midgard does not support warps. */
    if (pid.get_gpu_family() == product_id::gpu_family::midgard)
        return 0;

    switch (pid) {
    case product_id_g31:
    case product_id_g68:
    case product_id_g51:
    case product_id_g71:
    case product_id_g72:
        return 4;
    case product_id_g52:
    case product_id_g76:
        return 8;
    case product_id_g57:
    case product_id_g57_2:
    case product_id_g77:
    case product_id_g78:
    case product_id_g310:
    case product_id_g510:
    case product_id_g610:
    case product_id_g710:
    case product_id_gtux:
    case product_id_gtux_2:
        return 16;
    }

    /* Must be arch/product style since it is not Midgard. */
    assert(pid.get_version_style() == product_id::version_style::arch_product_major);

    /* Newer GPUs are likely to have wide wraps. */
    if (pid.get_arch_major() > 11)
        return 16;

    ec = std::make_error_code(std::errc::not_supported);
    return 0;
}

class prop_decoder {
  public:
    explicit prop_decoder(properties buffer) noexcept
        : reader_{std::move(buffer)} {}

    constants operator()(std::error_code &ec) noexcept {
        constants dev_consts{};
        uint64_t num_core_groups{};
        std::array<uint64_t, 16> core_mask{};

        while (reader_.size() > 0) {
            auto p = next(ec);
            prop_id_type id = p.first;
            uint64_t value = p.second;
            if (ec)
                return {};

            switch (id) {
            case prop_id_type::product_id:
                dev_consts.gpu_id = value;
                dev_consts.warp_width = get_warp_width(value, ec);
                if (ec)
                    return {};
                break;
            case prop_id_type::l2_log2_cache_size:
                dev_consts.l2_slice_size = (1UL << value);
                break;
            case prop_id_type::l2_num_l2_slices:
                dev_consts.num_l2_slices = value;
                break;
            case prop_id_type::raw_l2_features:
                /* log2(bus width in bits) stored in top 8 bits of register. */
                dev_consts.axi_bus_width = 1UL << ((value & 0xFF000000) >> 24);
                break;
            case prop_id_type::coherency_num_core_groups:
                num_core_groups = value;
                break;
            case prop_id_type::coherency_group_0:
                core_mask[0] = value;
                break;
            case prop_id_type::coherency_group_1:
                core_mask[1] = value;
                break;
            case prop_id_type::coherency_group_2:
                core_mask[2] = value;
                break;
            case prop_id_type::coherency_group_3:
                core_mask[3] = value;
                break;
            case prop_id_type::coherency_group_4:
                core_mask[4] = value;
                break;
            case prop_id_type::coherency_group_5:
                core_mask[5] = value;
                break;
            case prop_id_type::coherency_group_6:
                core_mask[6] = value;
                break;
            case prop_id_type::coherency_group_7:
                core_mask[7] = value;
                break;
            case prop_id_type::coherency_group_8:
                core_mask[8] = value;
                break;
            case prop_id_type::coherency_group_9:
                core_mask[9] = value;
                break;
            case prop_id_type::coherency_group_10:
                core_mask[10] = value;
                break;
            case prop_id_type::coherency_group_11:
                core_mask[11] = value;
                break;
            case prop_id_type::coherency_group_12:
                core_mask[12] = value;
                break;
            case prop_id_type::coherency_group_13:
                core_mask[13] = value;
                break;
            case prop_id_type::coherency_group_14:
                core_mask[14] = value;
                break;
            case prop_id_type::coherency_group_15:
                core_mask[15] = value;
                break;
            case prop_id_type::num_exec_engines:
                /* TODO GPUCORE-33051: Fix incorrect num_exec_engines in DDK kernel */
                dev_consts.num_exec_engines = 0;
                break;
            case prop_id_type::minor_revision:
            case prop_id_type::major_revision:
            default:
                break;
            }
        }
        for (auto i{0U}; i < num_core_groups; ++i)
            dev_consts.shader_core_mask |= core_mask[i];
        dev_consts.num_shader_cores = __builtin_popcount(dev_consts.shader_core_mask);

        dev_consts.tile_size = 16;

        return dev_consts;
    }

  private:
    /** Property id type. */
    using prop_id_type = ioctl::kbase::get_gpuprops::gpuprop_code;
    /** Property size type. */
    using prop_size_type = ioctl::kbase::get_gpuprops::gpuprop_size;

    static std::pair<prop_id_type, prop_size_type> to_prop_metadata(uint32_t v) noexcept {
        /* Property id/size encoding is:
         * +--------+----------+
         * | 31   2 | 1      0 |
         * +--------+----------+
         * | PropId | PropSize |
         * +--------+----------+
         */
        static constexpr unsigned prop_id_shift = 2;
        static constexpr unsigned prop_size_mask = 0x3;

        return {static_cast<prop_id_type>(v >> prop_id_shift), static_cast<prop_size_type>(v & prop_size_mask)};
    }

    std::pair<prop_id_type, uint64_t> next(std::error_code &ec) noexcept {
        /* Swallow any bad data. */
#define ERROR_IF_READER_SIZE_LT(x)                                                                                     \
    do {                                                                                                               \
        if (reader_.size() < (x)) {                                                                                    \
            ec = std::make_error_code(std::errc::protocol_error);                                                      \
            return {};                                                                                                 \
        }                                                                                                              \
    } while (0)

        ERROR_IF_READER_SIZE_LT(sizeof(uint32_t));

        auto p = to_prop_metadata(reader_.u32());
        prop_id_type id = p.first;
        prop_size_type size = p.second;

        switch (size) {
        case prop_size_type::uint8:
            ERROR_IF_READER_SIZE_LT(sizeof(uint8_t));
            return {id, reader_.u8()};
        case prop_size_type::uint16:
            ERROR_IF_READER_SIZE_LT(sizeof(uint16_t));
            return {id, reader_.u16()};
        case prop_size_type::uint32:
            ERROR_IF_READER_SIZE_LT(sizeof(uint32_t));
            return {id, reader_.u32()};
        case prop_size_type::uint64:
            ERROR_IF_READER_SIZE_LT(sizeof(uint64_t));
            return {id, reader_.u64()};
        }

#undef ERROR_IF_READER_SIZE_LT
        return {};
    }

    /* Reads values out of the KBASE_IOCTL_GET_GPUPROPS data buffer */
    class prop_reader {
      public:
        explicit prop_reader(properties buffer) noexcept
            : buffer_{std::move(buffer)}
            , data_{buffer_.data()}
            , size_{buffer_.size()} {}

        uint8_t u8() noexcept { return le_read_bytes<uint8_t>(); }
        uint16_t u16() noexcept { return le_read_bytes<uint16_t>(); }
        uint32_t u32() noexcept { return le_read_bytes<uint32_t>(); }
        uint64_t u64() noexcept { return le_read_bytes<uint64_t>(); }

        size_t size() const noexcept { return size_; }

      private:
        template <typename u_t>
        u_t le_read_bytes() noexcept {
            u_t ret{};
            for (auto b{0U}; b < sizeof(u_t); ++b)
                ret |= static_cast<u_t>(static_cast<uint64_t>(data_[b]) << (8 * b));
            data_ += sizeof(u_t);
            size_ -= sizeof(u_t);
            return ret;
        }

        properties const buffer_;
        unsigned char const *data_;
        std::size_t size_;
    } reader_;
};

} // namespace detail

template <typename syscall_iface_t>
class instance_impl : public instance, private syscall_iface_t {
    using kbase_version_type = ::hwcpipe::device::kbase_version;

  public:
    instance_impl(int fd, const syscall_iface_t &iface = {})
        : syscall_iface_t(iface)
        , fd_(fd) {
        std::error_code ec = init();
        if (ec) {
            valid_ = false;
            return;
        }

        const product_id pid{constants_.gpu_id};

        switch (backend_type_) {
        case hwcnt::backend_type::vinstr:
        case hwcnt::backend_type::vinstr_pre_r21: {
            block_extents_ = hwcnt::sampler::vinstr::construct_block_extents(pid, constants_.num_l2_slices,
                                                                             constants_.num_shader_cores);
            break;
        }
        case hwcnt::backend_type::kinstr_prfcnt:
        case hwcnt::backend_type::kinstr_prfcnt_wa:
        case hwcnt::backend_type::kinstr_prfcnt_bad:
            using namespace hwcnt::sampler::kinstr_prfcnt;
            std::tie(ec, ei_) = enum_info_parser::parse_enum_info(fd_, iface);
            if (ec) {
                valid_ = false;
                return;
            }
            block_extents_ = construct_block_extents(ei_);
            break;
        }
    }

    ~instance_impl() override = default;

    constants get_constants() const override { return constants_; }

    hwcnt::block_extents get_hwcnt_block_extents() const override { return block_extents_; }

    hwcnt::sampler::kinstr_prfcnt::enum_info get_enum_info() const {
        // enum info must have been initialized
        assert(ei_.num_values != 0);
        return ei_;
    }

    /**
     * Check if instance_impl is valid.
     *
     * @return True if valid, false otherwise.
     */
    bool valid() const { return valid_; }

    /**
     * Get file descriptor.
     *
     * @return The file descriptor for this instance_impl.
     */
    int fd() const { return fd_; }

    kbase_version_type kbase_version() const { return kbase_version_; }

    hwcnt::backend_type backend_type() const { return backend_type_; }

  private:
    /** @return Syscall iface reference. */
    syscall_iface_t &get_syscall_iface() { return *this; }

    /** Get device constants from old ioctl. */
    constants props_pre_r21(int fd, std::error_code &ec) {
        constants dev_consts{};

        ioctl::kbase_pre_r21::uk_gpuprops props{};
        props.header.id = ioctl::kbase_pre_r21::header_id::get_props;

        std::tie(ec, std::ignore) = get_syscall_iface().ioctl(fd, ioctl::kbase_pre_r21::command::get_gpuprops, &props);
        if (ec)
            return {};

        dev_consts.gpu_id = props.props.core_props.product_id;
        dev_consts.warp_width = detail::get_warp_width(dev_consts.gpu_id, ec);
        if (ec)
            return {};
        dev_consts.l2_slice_size = 1UL << props.props.l2_props.log2_cache_size;
        dev_consts.num_l2_slices = props.props.l2_props.num_l2_slices;
        dev_consts.axi_bus_width = 1UL << ((props.props.raw_props.l2_features & 0xFF000000) >> 24);

        for (auto i{0U}; i < props.props.coherency_info.num_core_groups; i++)
            dev_consts.shader_core_mask |= props.props.coherency_info.group[i].core_mask;
        dev_consts.num_shader_cores = __builtin_popcount(dev_consts.shader_core_mask);

        dev_consts.tile_size = 16;

        return dev_consts;
    }

    /** Get the raw properties buffer as it's returned from the kernel. */
    properties props_post_r21(int fd, std::error_code &ec) {
        int ret = 0;

        ioctl::kbase::get_gpuprops get_props = {};
        std::tie(ec, ret) = get_syscall_iface().ioctl(fd, ioctl::kbase::command::get_gpuprops, &get_props);
        if (ec)
            return {};

        get_props.size = static_cast<uint32_t>(ret);
        properties buffer(static_cast<std::size_t>(ret));
        get_props.buffer.reset(buffer.data());
        std::tie(ec, ret) = get_syscall_iface().ioctl(fd, ioctl::kbase::command::get_gpuprops, &get_props);
        if (ec)
            return {};

        return buffer;
    }

    /** Get CSF firmware version. It returns 0 for JM GPUs. */
    uint64_t get_fw_version() {
        if (kbase_version_.type() != ioctl_iface_type::csf)
            return 0;

        int ret = 0;
        std::error_code _;
        ioctl::kbase::cs_get_glb_iface get_glb{};

        std::tie(_, ret) = get_syscall_iface().ioctl(fd_, ioctl::kbase::command::cs_get_glb_iface, &get_glb);
        if (ret < 0)
            return 0;

        return get_glb.out.glb_version;
    }

    /** Detect kbase version and initialize kbase_version_ field. */
    std::error_code version_check() {
        std::error_code ec;
        ioctl_iface_type iface_type{ioctl_iface_type::csf};

        {
            ioctl::kbase::version_check version_check_args = {0, 0};

            std::tie(ec, std::ignore) =
                get_syscall_iface().ioctl(fd_, ioctl::kbase::command::version_check_csf, &version_check_args);

            if (ec) {
                iface_type = ioctl_iface_type::jm_post_r21;
                std::tie(ec, std::ignore) =
                    get_syscall_iface().ioctl(fd_, ioctl::kbase::command::version_check_jm, &version_check_args);
            }

            if (!ec) {
                kbase_version_ = kbase_version_type(version_check_args.major, version_check_args.minor, iface_type);
                return ec;
            }
        }

        ioctl::kbase_pre_r21::version_check_args version_check_args{};
        version_check_args.header.id = ioctl::kbase_pre_r21::header_id::version_check;

        std::tie(ec, std::ignore) =
            get_syscall_iface().ioctl(fd_, ioctl::kbase_pre_r21::command::version_check, &version_check_args);

        if (ec)
            return ec;

        iface_type = ioctl_iface_type::jm_pre_r21;
        kbase_version_ = kbase_version_type(version_check_args.major, version_check_args.minor, iface_type);

        constexpr kbase_version_type legacy_min_version{10, 2, ioctl_iface_type::jm_pre_r21};
        if (kbase_version_ < legacy_min_version)
            return std::make_error_code(std::errc::not_supported);

        return {};
    }

    /** Detect backend interface type and initialize backend_type_ field. */
    std::error_code backend_type_probe() {
        std::error_code ec;

        auto available_types = hwcnt::backend_type_discover(kbase_version_, product_id{constants_.gpu_id});
        std::tie(ec, backend_type_) = hwcnt::backend_type_select(available_types);

        return ec;
    }

    /** Call set flags ioctl. */
    std::error_code set_flags() {
        static constexpr auto system_monitor_flag_submit_disabled_bit = 1;
        static constexpr auto system_monitor_flag = 1U << system_monitor_flag_submit_disabled_bit;

        std::error_code ec;

        if (kbase_version_.type() == ioctl_iface_type::jm_pre_r21) {
            ioctl::kbase_pre_r21::set_flags_args flags{};
            flags.header.id = ioctl::kbase_pre_r21::header_id::set_flags;
            flags.create_flags = system_monitor_flag;

            std::tie(ec, std::ignore) =
                get_syscall_iface().ioctl(fd_, ioctl::kbase_pre_r21::command::set_flags, &flags);

            return ec;
        }

        ioctl::kbase::set_flags flags{system_monitor_flag};
        std::tie(ec, std::ignore) = get_syscall_iface().ioctl(fd_, ioctl::kbase::command::set_flags, &flags);

        return ec;
    }

    /** Initialize constants_ field. */
    std::error_code init_constants() {
        std::error_code ec;
        if (kbase_version_.type() == ioctl_iface_type::jm_pre_r21) {
            constants_ = props_pre_r21(fd_, ec);
            return ec;
        }

        auto p = props_post_r21(fd_, ec);
        if (ec)
            return {};

        constants_ = detail::prop_decoder{p}(ec);
        if (ec)
            return ec;

        constants_.fw_version = get_fw_version();

        return ec;
    }

    /** Get a raw properties buffer into structured data. */
    std::error_code init() {
        std::error_code ec = version_check();
        if (ec)
            return ec;

        static const std::error_code eperm = std::make_error_code(std::errc::operation_not_permitted);
        static const std::error_code einval = std::make_error_code(std::errc::invalid_argument);

        ec = set_flags();

        /* Note, that set_flags may fail with eperm if context has been already initialized. */
        if (ec && ec != eperm && ec != einval)
            return ec;

        ec = init_constants();
        if (ec)
            return ec;

        ec = backend_type_probe();
        if (ec)
            return ec;

        return {};
    }

    constants constants_{};
    hwcnt::block_extents block_extents_{};
    kbase_version_type kbase_version_{};
    hwcnt::backend_type backend_type_{};
    hwcnt::sampler::kinstr_prfcnt::enum_info ei_{};

    bool valid_{true};
    int fd_;
};

} // namespace device
} // namespace hwcpipe
