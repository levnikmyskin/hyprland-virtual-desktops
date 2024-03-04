{
  description = "A plugin for the Hyprland compositor, implementing virtual-desktop functionality.";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  inputs.hyprland.url = "github:hyprwm/Hyprland/v0.36.0";

  outputs = { self, nixpkgs, hyprland }:
    let
      # Helper function to create packages for each system
      withPkgsFor = fn: nixpkgs.lib.genAttrs (builtins.attrNames hyprland.packages) (system: fn system nixpkgs.legacyPackages.${system});
      virtualDesktops = withPkgsFor (system: pkgs: pkgs.gcc13Stdenv.mkDerivation rec {
        pname = "virtual-desktops";
        version = "2.2.0";
        src = ./.;
        
        inherit (hyprland.packages.${system}.hyprland) nativeBuildInputs;

        buildInputs = [ hyprland.packages.${system}.hyprland ] ++ hyprland.packages.${system}.hyprland.buildInputs;

        # Skip meson phases
        configurePhase = "true";
        mesonConfigurePhase  = "true";
        mesonBuildPhase = "true";
        mesonInstallPhase = "true";

        buildPhase = ''
          make all
        '';

        installPhase = ''
          mkdir -p $out/lib
          cp virtual-desktops.so $out/lib/libvirtual-desktops.so
        '';

        meta = with pkgs.lib; {
          homepage = "https://github.com/levnikmyskin/hyprland-virtual-desktops";
          description = "A plugin for the Hyprland compositor, implementing virtual-desktop functionality.";
          license = licenses.bsd3;
          platforms = platforms.linux;
        };
      });
    in
    {
      packages = withPkgsFor (system: pkgs: rec {
        virtual-desktops = virtualDesktops.${system};
        default = virtual-desktops;
      });

      devShells = withPkgsFor (system: pkgs: {
        default = pkgs.mkShell.override { stdenv = pkgs.gcc13Stdenv; } {
          name = "virtual-desktops";
          buildInputs = [ hyprland.packages.${system}.hyprland ];
          inputsFrom = [ hyprland.packages.${system}.hyprland ];
        };
      });
    };
}
