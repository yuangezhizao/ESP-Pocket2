# ESP-Pocket2 Cloud Agent 环境镜像
#
# 目标：把开发环境「代码化」，不依赖任何人的个人快照（Personal snapshot），任何人从带本文件的分支起 Cloud Agent 都能得到一致、可复现的环境。
#
# 说明：
# - Cloud Agent 本身就运行在「一个容器」里（Firecracker microVM 内的容器）；本 Dockerfile 只是「声明这个容器长什么样」，并非 docker-in-docker。未来要装 Tailscale / cloudflared，直接在本文件里 apt 安装或下载二进制、运行时按官方 userspace 方式启动即可，全程无需 docker-in-docker。
# - 基于 Espressif 官方镜像 espressif/idf:v5.5.4（发布 tag，可复现）：已内置 ESP-IDF v5.5.4、交叉编译器、Python venv、qemu-system-xtensa；IDF_PATH=/opt/esp/idf，IDF_TOOLS_PATH=/opt/esp。
# - 官方镜像是 Ubuntu based，满足 Cursor computer use（Desktop）对 Debian/Ubuntu 的要求。
# - 不要 COPY 项目源码：Cursor 会自行 checkout 正确的 commit 到工作区。
FROM espressif/idf:v5.5.4

# ┌───────────────────────────────────────────────────────────────────────────┐
# │ 平台会「自动安装」的包（无需在本文件重复安装）                              │
# │                                                                            │
# │ Cursor 每次启动都会跑桌面初始化，按容器内                                    │
# │   /usr/local/share/vnc-desktop.Aptfile                                      │
# │ 用 `apt-get install --no-install-recommends` 安装下面这些（实测清单）：      │
# │                                                                            │
# │   VNC     : tigervnc-standalone-server tigervnc-common tigervnc-tools       │
# │   XFCE    : xfce4 xfce4-terminal xfce4-settings thunar                      │
# │   X11     : x11-utils x11-xserver-utils xdg-utils xdotool xclip procps      │
# │   D-Bus   : dbus-x11 at-spi2-core                                           │
# │   应用    : mousepad seahorse sudo ffmpeg                                   │
# │   主题    : adwaita-icon-theme gnome-themes-extra gnome-keyring plank       │
# │             sassc libglib2.0-dev-bin libxml2-utils dconf-cli xz-utils       │
# │   locale  : locales                                                        │
# │   字体    : fonts-noto fonts-wqy-microhei fonts-droid-fallback              │
# │             fonts-noto-color-emoji fonts-liberation fonts-croscore          │
# │             fonts-cantarell fonts-jetbrains-mono xfonts-base xfonts-terminus│
# │   桌面库  : libx11-dev libxkbfile-dev libsecret-1-dev libgbm-dev            │
# │             libnotify4 libnss3 libxss1 libgl1-mesa-dri libglx-mesa0 libgl1  │
# │   Python  : python3-minimal python3-numpy                                   │
# │   浏览器  : google-chrome-stable（单独脚本 install-google-chrome 安装）      │
# │                                                                            │
# │ 另外平台还会：把 gh 软链到 /usr/local/bin/gh（→/exec-daemon/gh）、          │
# │ 配置 git 身份与 SSH 提交签名、每次启动 `git clean -fd` 并重建               │
# │ /opt/cursor/artifacts。                                                     │
# │                                                                            │
# │ ⚠️ 维护提示：若将来 Cursor 不再自动装上述包（表现为桌面/中文/               │
# │    `idf.py qemu --graphics` 异常），把缺的项补进下面的 apt-get install。    │
# └───────────────────────────────────────────────────────────────────────────┘

# 只安装「基础镜像缺失、或需保底内置」的包（sudo 属保底，其余为平台 Aptfile 未含）：
# - sudo       : Cursor 官方要求 Dockerfile 具备 git+sudo（git 官方镜像已含；sudo 很小，保底）
# - libslirp0  : idf.py qemu 用户态网络（-nic user,model=open_eth）所需（不在平台 Aptfile 内，必须自己装）
# - 开发 CLI   : htop vim less file lsof jq net-tools iproute2 dnsutils
# - ccache / dfu-util / git-lfs : 编译加速 / 设备烧录 / git 大文件
RUN apt-get update && apt-get install -y --no-install-recommends \
      sudo libslirp0 \
      htop vim less file lsof jq \
      net-tools iproute2 dnsutils \
      ccache dfu-util git-lfs \
    && rm -rf /var/lib/apt/lists/*

# 让交互式（非登录）shell 自动进入 ESP-IDF 环境（agent 的 shell 未必经过镜像 ENTRYPOINT），等价于快照环境里 ~/.bashrc 中 source export.sh 的效果。
# 注意：/etc/bash.bashrc 仅对交互式 bash 生效；非交互脚本（如 bash -c、CI）不读取它，需要时请显式 source。
RUN echo '. /opt/esp/idf/export.sh >/dev/null 2>&1 || true' >> /etc/bash.bashrc
