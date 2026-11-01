# Qualified Ui patch

The renderer builds Ui base commit f88aab5150ca7f375b8c4a61a01da57832c82e03 with
the focused compact-colour-picker correction in compact-color-picker.patch.
The same correction is applied to the shared Ui control source; it does not
change Dropdown, Toggle, Core or Animation.

Before raw SHA-256: f50b16dbb638a9b889bb5b94ba57cb2e371cf5c8c02f2140e54093ac2fe33f66.
After raw SHA-256: 3e3726e2eb7af21bdde18e03f2806faeb80a2fc1c4c6e7e713665f193abcc7fa.
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

The follow-up check covers the Colour page too: six readout edits inherited
the full-size style inside 30-pixel rows requiring 36 pixels. Their compact recipe now
updates with the theme and their row heights are measured.
Changing to five-channel CMYK now relayouts the parent stack immediately.
All seven channel modes are checked in both themes, alongside all four pages.

The Roles & states workbench consumes the original Shared/UiTheme package from
upp_cinespace read-only. Its exact compiled header/source/package hashes are
recorded in the current qualification evidence; no theme code was duplicated.
