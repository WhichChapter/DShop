# DShop

DShop is an independent homebrew catalogue for PlayStation Vita.

## Live catalogue

The Vita client fetches this file over HTTPS every time it starts, and TRIANGLE refreshes it:

`https://raw.githubusercontent.com/WhichChapter/DShop/main/catalog/catalog.json`

Add one object per listing inside the `apps` array. Example:

```json
{
  "schema_version": 1,
  "store": "DShop",
  "catalogue_version": 2,
  "apps": [
    {
      "id": "example-homebrew",
      "title": "Example Homebrew",
      "description": "A short description shown in the store.",
      "category": "Games",
      "version": "1.0",
      "price": "Free",
      "vpk_url": "https://example.com/downloads/example.vpk",
      "homepage": "https://example.com/"
    }
  ]
}
```

Required fields: `title` and `vpk_url`. Optional fields: `id`, `description`, `category`, `version`, `price`, and `homepage`. Use a direct HTTPS link to the VPK, such as a GitHub Release asset URL. The client currently displays catalogue entries and their VPK links; downloading and installing directly inside DShop is a later milestone.

## Build

GitHub Actions builds the VPK using VitaSDK. The output is `DShop.vpk` in the `DShop-VPK` workflow artifact.

## Distribution

Only list software where redistribution is authorised by its developer. Prefer original project pages and release links. The client currently requires a valid HTTPS certificate for the catalogue request.
