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
, virtviewer
, tigervnc

, withDbus ? false
, withNetworkMap ? false
, withOVF ? true
, withSnapshots ? false
, withSpice ? true
, withUTF ? false
, withVNC ? true

, configName ? ".config/nemu/nemu.cfg"
, vmDir ? ".local/share/nemu/vms"
, databaseName ? ".local/share/nemu/nemu.db"
}:

stdenv.mkDerivation rec {
  pname = "nemu";
  version = "2021-04-01";

  src = fetchFromGitHub {
    owner = "nemuTUI";
    repo = "nemu";
    rev = "9fff326adc1c5fc27cddeedc2c091e5bb2a24347";
    sha256 = "10gis2fckjhj4ln9gb02qbcin82rx98fi6zh0wi5k1xpj8bmqcci";
  };

  qemu_ = if withSnapshots
    then qemu.overrideAttrs (attrs: {
      patches = attrs.patches ++ [
        (fetchpatch {
          url = "https://raw.githubusercontent.com/nemuTUI/nemu/master/patches/qemu-qmp-savevm-5.0.0+.patch";
          sha256 = "07w18h61m282f2nllxjxzv92lnvz61y9la6p8w7haj6a00kyispn";
        })
      ];
    })
    else qemu;

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
    qemu_
    ncurses
    socat
    picocom
  ]
    ++ lib.optional withDbus dbus
    ++ lib.optional withNetworkMap graphviz
    ++ lib.optionals withOVF [ libxml2 libarchive ]
    ++ lib.optional withSpice virtviewer
    ++ lib.optional withVNC tigervnc;

  cmakeFlags = [
    "-DNM_CFG_NAME=${configName}"
    "-DNM_DEFAULT_VMDIR=${vmDir}"
    "-DNM_DEFAULT_VNC=${tigervnc}/bin/vncviewer"
    "-DNM_DEFAULT_DBFILE=${databaseName}"
    "-DNM_DEFAULT_SPICE=${virtviewer}/bin/remote-viewer"
    "-DNM_DEFAULT_QEMUDIR=${qemu_}/bin"
  ]
    ++ lib.optional withDbus "-DNM_WITH_DBUS=ON"
    ++ lib.optional withNetworkMap "-DNM_WITH_NETWORK_MAP=ON"
    ++ lib.optional withOVF "-DNM_WITH_OVF_SUPPORT=ON"
    ++ lib.optional withSnapshots "-DNM_SAVEVM_SNAPSHOTS=ON"
    ++ lib.optional withSpice "-DNM_WITH_SPICE=ON"
    ++ lib.optional withUTF "-DNM_USE_UTF=ON"
    ++ lib.optional withVNC "-DNM_WITH_VNC_CLIENT=ON";

  preConfigure = ''
    patchShebangs .

    substituteInPlace lang/ru/nemu.po --replace \
      /bin/false /run/current-system/sw/bin/false

    substituteInPlace nemu.cfg.sample --replace \
      /usr/bin/vncviewer ${tigervnc}/bin/vncviewer

    substituteInPlace nemu.cfg.sample --replace \
      /usr/bin/remote-viewer ${virtviewer}/bin/remote-viewer

    substituteInPlace nemu.cfg.sample --replace \
      "qemu_bin_path = /usr/bin" "qemu_bin_path = ${qemu_}/bin"

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
