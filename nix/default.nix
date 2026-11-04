{
  lib,
  pkgs,
  waybar,
  version,
}:
let
  libcava = rec {
    # NOTE: Needs to match libcava.wrap, which expects the source in subprojects/cava-<rev>.
    rev = "8d6e963db9cd29c2b3b5a5df1e46bb65c410f9cc";
    src = pkgs.fetchFromGitHub {
      owner = "LukashonakV";
      repo = "cava";
      inherit rev;
      hash = "sha256-tWY3D3sEVFDHJjO/enCu0lEobM1CF4vupavOHgIslnA=";
    };
  };
in
waybar.overrideAttrs (oldAttrs: {
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

  buildInputs =
    (builtins.filter (p: p.pname != "wireplumber" && p.pname != "gps") oldAttrs.buildInputs)
    ++ [
      pkgs.wireplumber
      pkgs.gpsd
      pkgs.modemmanager
    ];

  postUnpack = ''
    pushd "$sourceRoot"
    cp -R --no-preserve=mode,ownership ${libcava.src} subprojects/cava-${libcava.rev}
    patchShebangs .
    popd
  '';
})
