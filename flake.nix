{
  description = "Digital Speech Decoder - Florida Man Edition ";

  inputs = {
    flake-parts.url = "github:hercules-ci/flake-parts";
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = inputs@{ flake-parts, ... }:
    flake-parts.lib.mkFlake { inherit inputs; } {
      systems = [ "x86_64-linux" "aarch64-linux" "aarch64-darwin" "x86_64-darwin" ];
      perSystem = { config, pkgs, ... }: {
        packages.default = config.packages.dsd-fme;

        packages.dsd-fme = pkgs.callPackage ./package.nix { };
      };
    };
}
