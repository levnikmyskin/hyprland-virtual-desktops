{
  description = "A plugin for the Hyprland compositor, implementing virtual-desktop functionality.";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs =
    {
      self,
      nixpkgs,
    }:
    let
      # Helper function to create packages for each system
      forAllSystems = nixpkgs.lib.genAttrs [
        "x86_64-linux"
        "aarch64-linux"
      ];

      virtualDesktops = forAllSystems (
        system: nixpkgs.legacyPackages.${system}.callPackage ./default.nix { }
      );
    in
    {
      packages = forAllSystems (system: rec {
        virtual-desktops = virtualDesktops.${system};
        default = virtual-desktops;
      });

      devShells = forAllSystems (
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in
        {
          default = pkgs.mkShell.override { stdenv = pkgs.gcc14Stdenv; } {
            name = "virtual-desktops";

            inherit (pkgs.hyprland) nativeBuildInputs;

            buildInputs =
              with pkgs;
              [
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
              ]
              ++ pkgs.hyprland.buildInputs;
          };
        }
      );
    };
}
