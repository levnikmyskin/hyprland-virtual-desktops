{
  description = "A plugin for the Hyprland compositor, implementing virtual-desktop functionality.";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = {
    self,
    nixpkgs,
  }: let
    # Helper function to create packages for each system
    forAllSystems = nixpkgs.lib.genAttrs ["x86_64-linux" "aarch64-linux"];
    
    virtualDesktops = forAllSystems (system: let
      pkgs = nixpkgs.legacyPackages.${system};
      hyprland = pkgs.hyprland;
    in
      pkgs.gcc14Stdenv.mkDerivation {
        pname = "virtual-desktops";
        version = "2.2.8";
        src = ./.;

        inherit (hyprland) nativeBuildInputs;

        buildInputs = with pkgs; [
          hyprland
          hyprland.dev
          libdrm
          libinput
          wayland
          wayland-protocols
          cairo
          pango
        ] ++ hyprland.buildInputs;

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

        meta = with pkgs.lib; {
          homepage = "https://github.com/levnikmyskin/hyprland-virtual-desktops";
          description = "A plugin for the Hyprland compositor, implementing virtual-desktop functionality.";
          license = licenses.bsd3;
          platforms = platforms.linux;
        };
      });
  in {
    packages = forAllSystems (system: rec {
      virtual-desktops = virtualDesktops.${system};
      default = virtual-desktops;
    });

    devShells = forAllSystems (system: let
      pkgs = nixpkgs.legacyPackages.${system};
    in {
      default = pkgs.mkShell.override {stdenv = pkgs.gcc14Stdenv;} {
        name = "virtual-desktops";

        inherit (pkgs.hyprland) nativeBuildInputs;

        buildInputs = with pkgs; [
          hyprland
          hyprland.dev
          wayland
          wayland-protocols
          cairo
          pango
          wlroots
          clang
          clang-tools
          nodejs
          include-what-you-use
          cpplint
        ];
      };
    });
  };
}
