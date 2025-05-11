{
  description = "A plugin for the Hyprland compositor, implementing virtual-desktop functionality.";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  # Hyprland at tag v0.49.0
  inputs.hyprland.url = "github:hyprwm/Hyprland/9958d297641b5c84dcff93f9039d80a5ad37ab00";

  outputs = {
    self,
    nixpkgs,
    hyprland,
  }: let
    # Helper function to create packages for each system
    withPkgsFor = fn: nixpkgs.lib.genAttrs (builtins.attrNames hyprland.packages) (system: fn system nixpkgs.legacyPackages.${system});
    virtualDesktops = withPkgsFor (system: pkgs:
      pkgs.gcc14Stdenv.mkDerivation {
        pname = "virtual-desktops";
        version = "2.2.8";
        src = ./.;

        inherit (hyprland.packages.${system}.hyprland) nativeBuildInputs;

        buildInputs = [hyprland.packages.${system}.hyprland] ++ hyprland.packages.${system}.hyprland.buildInputs;

        # Skip meson phases
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
    packages = withPkgsFor (system: pkgs: rec {
      virtual-desktops = virtualDesktops.${system};
      default = virtual-desktops;
    });

    devShells = withPkgsFor (system: pkgs: {
      default = pkgs.mkShell.override {stdenv = pkgs.gcc14Stdenv;} {
        name = "virtual-desktops";
        inputsFrom = [hyprland.packages.${system}.hyprland];
        nativeBuildInputs = [pkgs.cmake];
        buildInputs = with pkgs; [
          clang
          clang-tools
          nodejs # for copilot
          include-what-you-use
          cpplint

          hyprland.packages.${system}.hyprland
        ];
      };
    });
  };
}
