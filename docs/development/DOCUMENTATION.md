# Documentation site

The GitHub Pages site uses [Zensical](https://zensical.org/). Edit the existing
Markdown guides under `docs/`; do not maintain a second copy for the website.
Keep pages short, use descriptive headings and link to detail rather than repeating it.

## Preview locally

```sh
python3 -m venv build/docs-venv
build/docs-venv/bin/python -m pip install -r requirements-docs.txt
build/docs-venv/bin/python tools/build_docs.py --serve
```

Open the local address printed by Zensical. After editing a guide, restart the
command to refresh its staged copy. Omit `--serve` to build `build/site/`.
On Windows, use `build/docs-venv/Scripts/python.exe` instead.

`zensical.toml` defines navigation and appearance. The build script stages only
reviewed documentation from `config/package-inputs.json`; links to source files
outside `docs/` point to GitHub. Generated files stay under ignored `build/`.

## Publish

In GitHub repository **Settings → Pages**, select **GitHub Actions** as the source.
The Documentation workflow checks pull requests and deploys pushes to `main`.
You can also run it manually from `main`. Pull requests never deploy.

The public address is `https://nextcode4u.github.io/gloam-shell-ndx/` after the
first successful deployment. Deployment is separate from publishing a Gloam release.
