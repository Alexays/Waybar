{
  description = "Highly customizable Wayland bar for Sway and Wlroots based compositors.";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    devshell.url = "github:numtide/devshell";
    flake-utils.url = "github:numtide/flake-utils";
    flake-compat = {
      url = "github:edolstra/flake-compat";
      flake = false;
    };
  };

  outputs = { self, flake-utils, devshell, nixpkgs, flake-compat }:
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
      overlays.default = final: prev: {
        waybar = final.callPackage ./nix/default.nix {
          version = prev.waybar.version + "+date=" + (mkDate (self.lastModifiedDate or "19700101")) + "_" + (self.shortRev or "dirty");
        };
      };
      packages = genSystems
        (system:
          (self.overlays.default pkgsFor.${system} pkgsFor.${system})
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
            # from nativeBuildInputs
            gnumake
            meson
            ninja
            pkg-config
            scdoc
          ] ++ (map lib.getDev [
            # from buildInputs
            wayland wlroots gtkmm3 libsigcxx jsoncpp spdlog gtk-layer-shell howard-hinnant-date libxkbcommon
            # optional dependencies
            gobject-introspection glib playerctl python3.pkgs.pygobject3
            libevdev libinput libjack2 libmpdclient playerctl libnl
            libpulseaudio sndio sway libdbusmenu-gtk3 udev upower wireplumber

            # from propagated build inputs?
            at-spi2-atk atkmm cairo cairomm catch2 fmt_8 fontconfig
            gdk-pixbuf glibmm gtk3 harfbuzz pango pangomm wayland-protocols
          ]);
          env = with pkgs; [
            { name = "CPLUS_INCLUDE_PATH"; prefix = "$DEVSHELL_DIR/include"; }
            { name = "PKG_CONFIG_PATH"; prefix = "$DEVSHELL_DIR/lib/pkgconfig"; }
            { name = "PKG_CONFIG_PATH"; prefix = "$DEVSHELL_DIR/share/pkgconfig"; }
            { name = "PATH"; prefix = "${wayland.bin}/bin"; }
            { name = "LIBRARY_PATH"; prefix = "${lib.getLib sndio}/lib"; }
            { name = "LIBRARY_PATH"; prefix = "${lib.getLib zlib}/lib"; }
            { name = "LIBRARY_PATH"; prefix = "${lib.getLib howard-hinnant-date}/lib"; }
          ];
        };
    });
}
