{
  description = "Highly customizable Wayland bar for Sway and Wlroots based compositors.";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    devshell.url = "github:numtide/devshell";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, flake-utils, devshell, nixpkgs }:
    let
      inherit (nixpkgs) lib;
      genSystems = lib.genAttrs [
        "x86_64-linux"
      ];

      pkgsFor = genSystems (system:
        import nixpkgs {
          inherit system;
        });

      mkDate = longDate: (lib.concatStringsSep "-" [
        (builtins.substring 0 4 longDate)
        (builtins.substring 4 2 longDate)
        (builtins.substring 6 2 longDate)
      ]);
    in
    {
      overlays.default = _: prev: rec {
        waybar = prev.callPackage ./nix/default.nix {
          version = "0.9.16" + "+date=" + (mkDate (self.lastModifiedDate or "19700101")) + "_" + (self.shortRev or "dirty");
        };
      };
      packages = genSystems
        (system:
          (self.overlays.default null pkgsFor.${system})
          // {
            default = self.packages.${system}.waybar;
          });
    } //
    flake-utils.lib.eachDefaultSystem (system: {
      devShell =
        let pkgs = import nixpkgs {
          inherit system;

          overlays = [ devshell.overlay ];
        };
        in
        pkgs.devshell.mkShell {
          imports = [ "${pkgs.devshell.extraModulesDir}/language/c.nix" ];
          commands = [
            {
              package = pkgs.devshell.cli;
              help = "Per project developer environments";
            }
          ];
          devshell.packages = with pkgs; [
            clang-tools
            gdb
          ];
          language.c.libraries = with pkgs; [
          ];
        };
    });
}
