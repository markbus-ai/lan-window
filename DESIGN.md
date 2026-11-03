---
name: LAN Window
description: A dark, quiet LAN screen-sender where colour marks connection state and the video leads.
colors:
  ground: "#15171a"
  surface: "#1e2126"
  raised: "#2a2e34"
  border: "#3a4048"
  text: "#e6e8eb"
  text-secondary: "#9aa1aa"
  text-disabled: "#8a919b"
  focus-primary: "#57a6ff"
typography:
  family: "Barlow (bundled, OFL)"
  body: "15px / 500"
  field-label: "14px / 500"
  sender-state: "22px / 700"
  placeholder: "24px / 500"
---

# LAN Window

The interface stays out of the way. Controls are quiet so the video, the part Discord captures, is what you look at.

## Colour

Colour belongs to state, as a tinted background with matching text:

| State | Background | Text |
| --- | --- | --- |
| Idle | `#2a2e34` | `#c3c8cf` |
| Waiting | `#3a3212` | `#f2cf5b` |
| Live | `#12351f` | `#57d98a` |
| Error | `#3d1a16` | `#ff8a75` |

The primary button is the only light mass on screen. Its focus ring, `#57a6ff`, is the only colour outside state.

## Layout

- **Sender:** state at the top, detail line with two reserved lines, form, full-width primary action.
- **Receiver:** one 36px control row above the video, hidden with F11.
- **Video surface:** pure black. With no video it shows only the state word; errors add their reason. No addresses.

## Components

Corners are 4px, or 3px on checkboxes. Borders are 1px with no shadows. Fields lighten their border on hover and turn near-white on focus. An invalid field uses the error colours and takes focus.

## Assets

- `assets/down-arrow.png`: `magick -size 28x20 xc:none -fill '#c3c8cf' -draw 'polygon 0,0 28,0 14,20'`
- `assets/check.png`: `magick -size 32x32 xc:none -fill none -stroke '#15171a' -strokewidth 5 -draw 'polyline 5,17 13,25 27,7'`
- `assets/fonts/`: Barlow Medium and Bold, with `OFL-Barlow.txt`.
