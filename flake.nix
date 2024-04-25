{
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-23.11";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
      in
      {
        devShells.default = pkgs.mkShell {
          packages = [
            pkgs.clang
            pkgs.cmake
            pkgs.ninja

            (pkgs.writeShellScriptBin "signet_gen_build" ''
              mkdir -p build
              ${pkgs.cmake}/bin/cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=${pkgs.clang}/bin/clang -DCMAKE_CXX_COMPILER=${pkgs.clang}/bin/clang++ -DCMAKE_EXPORT_COMPILE_COMMANDS=ON 
            '')

            (pkgs.writeShellScriptBin "signet_build" ''
              mkdir -p build
              ${pkgs.cmake}/bin/cmake --build build
            '')
          ];
          shellHook = ''
          '';
        };
      });
}
