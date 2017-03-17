{ nixpkgs ? import <nixpkgs> {} }:

nixpkgs.pkgs.qt57.callPackage
  (import ./.)
  { }
