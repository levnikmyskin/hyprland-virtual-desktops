{
  lib,
  gcc14Stdenv,
  hyprland,
  libinput,
  wayland,
  wayland-protocols,
  cairo,
  pango,
}:
gcc14Stdenv.mkDerivation {
  pname = "virtual-desktops";
  version = "2.2.8";
  src = ./.;

  inherit (hyprland) nativeBuildInputs;

  buildInputs = [
    hyprland
    hyprland.dev
    libinput
    wayland
    wayland-protocols
    cairo
    pango
  ]
  ++ hyprland.buildInputs;

  # Skip meson phases since we're using make
  configurePhase = "true";
  mesonConfigurePhase = "true";
  mesonBuildPhase = "true";
  mesonInstallPhase = "true";

  buildPhase = ''
    runHook preBuild
    make all
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/lib
    cp virtual-desktops.so $out/lib/libvirtual-desktops.so
    runHook postInstall
  '';

  meta = with lib; {
    homepage = "https://github.com/levnikmyskin/hyprland-virtual-desktops";
    description = "A plugin for the Hyprland compositor, implementing virtual-desktop functionality.";
    license = licenses.bsd3;
    platforms = platforms.linux;
  };
}
