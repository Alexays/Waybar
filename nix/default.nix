{
  lib,
  pkgs,
  waybar,
  version,
}:
let
  libcava = rec {
    version = "0.10.4";
    src = pkgs.fetchFromGitHub {
      owner = "LukashonakV";
      repo = "cava";
      tag = version;
      hash = "sha256-9eTDqM+O1tA/3bEfd1apm8LbEcR9CVgELTIspSVPMKM=";
    };
  };
in
(waybar.overrideAttrs (oldAttrs: {
  inherit version;

  src = lib.cleanSourceWith {
    filter = name: type: type != "regular" || !lib.hasSuffix ".nix" name;
    src = lib.cleanSource ../.;
  };

  mesonFlags = lib.remove "-Dgtk-layer-shell=enabled" oldAttrs.mesonFlags;

  # downstream patch should not affect upstream
  patches = [ ];
  # nixpkgs checks version, no need when building locally
  nativeInstallCheckInputs = [ ];

  buildInputs = (builtins.filter (p:
    p.pname != "wireplumber" &&
    p.pname != "gps"
  ) oldAttrs.buildInputs) ++ [
    pkgs.wireplumber
    pkgs.gpsd
  ];

  postUnpack = ''
    pushd "$sourceRoot"
    cp -R --no-preserve=mode,ownership ${libcava.src} subprojects/cava-${libcava.version}
    patchShebangs .
    popd
  '';
}))
