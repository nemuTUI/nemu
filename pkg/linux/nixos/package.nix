{ stdenv, config, lib, fetchFromGitHub, fetchpatch, installShellFiles
, cmake
, pkg-config
, gettext
, libpthreadstubs
, libudev
, libusb1
, sqlite
, qemu
, ncurses
, socat
, picocom

, dbus
, graphviz
, libxml2
, libarchive
, json_c
, virtviewer
, tigervnc
, openssl

, withDbus ? false
, withNetworkMap ? false
, withOVF ? true
, withSpice ? true
, withVNC ? true
, withRemote ? false

, configName ? ".config/nemu/nemu.cfg"
, vmDir ? ".local/share/nemu/vms"
, databaseName ? ".local/share/nemu/nemu.db"
}:

stdenv.mkDerivation rec {
  pname = "nemu";
  version = "unstable-2021-09-03";

  src = fetchFromGitHub {
    owner = "nemuTUI";
    repo = "nemu";
    rev = "ce526ba4e6a674c193f0d4365bf0d95c45a6ef45";
    sha256 = "1mbg1pm10vyvy3kwkkilx466srfysqalyy2fcx5i5dxmc1zymr33";
  };

  system.requiredKernelConfig = with config.lib.kernelConfig; [
    (isEnabled "VETH")
    (isEnabled "MACVTAP")
  ];

  nativeBuildInputs = [ cmake pkg-config installShellFiles ];

  buildInputs = [
    gettext
    libpthreadstubs
    libudev
    libusb1
    sqlite
    qemu
    ncurses
    socat
    picocom
    json_c
  ]
    ++ lib.optional withDbus dbus
    ++ lib.optional withNetworkMap graphviz
    ++ lib.optionals withOVF [ libxml2 libarchive ]
    ++ lib.optional withSpice virtviewer
    ++ lib.optional withVNC tigervnc
    ++ lib.optional withRemote openssl;

  cmakeFlags = [
    "-DNM_CFG_NAME=${configName}"
    "-DNM_DEFAULT_VMDIR=${vmDir}"
    "-DNM_DEFAULT_VNC=${tigervnc}/bin/vncviewer"
    "-DNM_DEFAULT_DBFILE=${databaseName}"
    "-DNM_DEFAULT_SPICE=${virtviewer}/bin/remote-viewer"
    "-DNM_DEFAULT_QEMUDIR=${qemu}/bin"
  ]
    ++ lib.optional withDbus "-DNM_WITH_DBUS=ON"
    ++ lib.optional withNetworkMap "-DNM_WITH_NETWORK_MAP=ON"
    ++ lib.optional withOVF "-DNM_WITH_OVF_SUPPORT=ON"
    ++ lib.optional withSpice "-DNM_WITH_SPICE=ON"
    ++ lib.optional withVNC "-DNM_WITH_VNC_CLIENT=ON"
    ++ lib.optional withRemote "-DNM_WITH_REMOTE=ON";

  preConfigure = ''
    patchShebangs .

    substituteInPlace lang/ru/nemu.po --replace \
      /bin/false /run/current-system/sw/bin/false

    substituteInPlace nemu.cfg.sample --replace \
      /usr/bin/vncviewer ${tigervnc}/bin/vncviewer

    substituteInPlace nemu.cfg.sample --replace \
      /usr/bin/remote-viewer ${virtviewer}/bin/remote-viewer

    substituteInPlace nemu.cfg.sample --replace \
      "qemu_bin_path = /usr/bin" "qemu_bin_path = ${qemu}/bin"

    substituteInPlace sh/ntty --replace \
      /usr/bin/socat ${socat}/bin/socat

    substituteInPlace sh/ntty --replace \
      /usr/bin/picocom ${picocom}/bin/picocom

    substituteInPlace sh/upgrade_db.sh --replace \
      sqlite3 ${sqlite}/bin/sqlite3

    substituteInPlace sh/setup_nemu_nonroot.sh --replace \
      /usr/bin/nemu $out/bin/$pname

    substituteInPlace src/nm_cfg_file.c --replace \
      /bin/false /run/current-system/sw/bin/false
  '';

  preInstall = ''
    install -D -m0644 -t $out/share/doc ../LICENSE
  '';

  postInstall = ''
    installShellCompletion --bash $out/share/nemu/scripts/nemu.bash
  '';

  meta = {
    homepage = "https://github.com/nemuTUI/nemu";
    description = "Ncurses UI for QEMU";
    license = lib.licenses.bsd2;
    platforms = with lib.platforms; linux;
  };
}
