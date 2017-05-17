{ nixpkgs ? import <nixpkgs> {} }:

nixpkgs.pkgs.libsForQt56.callPackage
  (import ./.)
  { }
