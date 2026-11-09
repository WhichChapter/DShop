# DShop

DShop is an independent homebrew store for PlayStation Vita.

The project is designed around three separate parts:

- **Vita client** — a native Vita application packaged as a VPK.
- **Catalogue** — a store-owned JSON catalogue hosted in this repository.
- **Releases** — downloadable VPKs and data archives hosted through GitHub Releases.

## Current status

The first milestone is a bootable native Vita application with a four-tab interface. The catalogue and download/install system will be added in later milestones.

## Build

The project is built automatically by GitHub Actions using VitaSDK.

To build locally with VitaSDK installed:

```sh
vdpm vita2d curl openssl zstd
cmake -S . -B build
cmake --build build --parallel
```

The VPK is generated at `build/DShop.vpk`.

## Catalogue format

The initial catalogue lives at `catalog/catalog.json`. Each app entry will eventually include an ID, title, description, category, version, icon URL, VPK URL, optional data archive URL, checksums, and source/release links.

## Safety and distribution

DShop will distribute homebrew and other software only where the developer has authorised redistribution. Store entries should link to original project pages and include checksums. The client must verify downloads before installation.
