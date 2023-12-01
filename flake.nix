{
  description = "Highly customizable Wayland bar for Sway and Wlroots based compositors.";

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
      ] (system: func (import nixpkgs { inherit system; }));

      mkDate = longDate: (lib.concatStringsSep "-" [
        (builtins.substring 0 4 longDate)
        (builtins.substring 4 2 longDate)
        (builtins.substring 6 2 longDate)
      ]);
    in
    {
      overlays.default = final: prev: {
        waybar = final.callPackage ./nix/default.nix {
          # take the first "version: '...'" from meson.build
          version =
            (builtins.head (builtins.split "'"
              (builtins.elemAt
                (builtins.split " version: '" (builtins.readFile ./meson.build))
                2)))
            + "+date=" + (mkDate (self.lastModifiedDate or "19700101")) + "_" + (self.shortRev or "dirty");
        };
      };
      packages = genSystems (pkgs:
        let packages = self.overlays.default pkgs pkgs;
        in packages // {
          default = packages.waybar;
        });
    } //
    genSystems (pkgs: {
      devShells.default =
        pkgs.mkShell {
          name = "waybar-shell";

          # most of these aren't actually used in the waybar derivation, this is just in case
          # they will ever start being used
          inherit (pkgs.waybar) buildInputs depsBuildBuild depsBuildBuildPropagated depsBuildTarget
            depsBuildTargetPropagated depsHostHost depsHostHostPropagated depsTargetTarget
            depsTargetTargetPropagated propagatedBuildInputs propagatedNativeBuildInputs strictDeps;

          nativeBuildInputs = pkgs.waybar.nativeBuildInputs ++ (with pkgs; [
            clang-tools
            gdb
          ]);
        };
    });
}
