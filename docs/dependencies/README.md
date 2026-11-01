# Qualified Ui patch

The renderer builds Ui base commit f88aab5150ca7f375b8c4a61a01da57832c82e03 with
the focused compact-colour-picker correction in compact-color-picker.patch.
The same correction is applied to the shared Ui control source; it does not
change Dropdown, Toggle, Core or Animation.

Before raw SHA-256: f50b16dbb638a9b889bb5b94ba57cb2e371cf5c8c02f2140e54093ac2fe33f66.
After raw SHA-256: 39f7f261173943a6c85b5d0f4f49af6128049d397494aba5df67a1c6ccb3000d.
These raw identities describe the maintainer's CRLF files; Git may normalize EOLs.

For a fresh Ui export, run git apply --check and then git apply against this
patch from the export root. Do not apply it twice or overwrite unrelated Ui work.
The release manifest identifies the qualified base and patch separately.

The regression reproduced themed minimum-size violations before the fix:
30px dropdowns vs36px minimum;24px edits vs36px;38x28 switches vs64x40.
The picker now owns compact child recipes, measures labels, refreshes those
recipes on theme changes and derives active page ink/underline from the accent.
The renderer qualification reads each visible field's public minimum-size
contract on the Image page in both Light and Dark.
