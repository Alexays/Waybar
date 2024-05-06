{
  description = "Highly customizable Wayland bar for Sway and Wlroots based compositors";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-compat = {
      url = "github:edolstra/flake-compat";
      flake = false;
    };
  };

  outputs = { self, nixpkgs, ... }:
    let
      inherit (nixpkgs) lib;
      genSystems = func: lib.genAttrs [
        "x86_64-linux"
        "aarch64-linux"
      ]
        (system: func (import nixpkgs {
          inherit system;
          overlays = with self.overlays; [
            waybar
          ];
        }));

      mkDate = longDate: (lib.concatStringsSep "-" [
        (builtins.substring 0 4 longDate)
        (builtins.substring 4 2 longDate)
        (builtins.substring 6 2 longDate)
      ]);
    in
    {
      devShells = genSystems
        (pkgs:
          {
            default =
              pkgs.mkShell
                {
                  name = "waybar-shell";

                  # inherit attributes from upstream nixpkgs derivation
                  inherit (pkgs.waybar) buildInputs depsBuildBuild depsBuildBuildPropagated depsBuildTarget
                    depsBuildTargetPropagated depsHostHost depsHostHostPropagated depsTargetTarget
                    depsTargetTargetPropagated propagatedBuildInputs propagatedNativeBuildInputs strictDeps;

                  # overrides for local development
                  nativeBuildInputs = pkgs.waybar.nativeBuildInputs ++ (with pkgs; [
                    clang-tools
                    gdb
                  ]);
                };
          });

      overlays = {
        default = self.overlays.waybar;
        waybar = final: prev: {
          waybar = final.callPackage ./nix/default.nix {
            waybar = prev.waybar;
            # take the first "version: '...'" from meson.build
            version =
              (builtins.head (builtins.split "'"
                (builtins.elemAt
                  (builtins.split " version: '" (builtins.readFile ./meson.build))
                  2)))
              + "+date=" + (mkDate (self.lastModifiedDate or "19700101")) + "_" + (self.shortRev or "dirty");
          };
        };
      };

      packages = genSystems (pkgs: {
        default = self.packages.${pkgs.stdenv.hostPlatform.system}.waybar;
        inherit (pkgs) waybar;
      });
    };
}
