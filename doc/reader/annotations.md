# Bookmarks and annotations

## Bookmarks

### Custom pin icons

- To add a custom pin icon on Kobo, create `.adds/kobalt/custom-pins/` and copy SVG files there over USB.

- On desktop, use `custom-pins/` under the app's existing profile directory.
- On Android, choose the icon folder in Library > More > Resource Folders. See [Android resource folders](android.md#resource-folders).
- Tap the question-mark button at the right of the pin editor heading for platform-specific instructions. If files fail to load, a warning replaces that button.
  - Tap it to see up to five failed filenames and a count of any others. The failed-file list also has a Help button. The picker uses one grid with shared dividers. Close and reopen the book after adding or replacing files.
- The pin editor uses one list, with custom icons first in filename order, followed by the built-in icons. Previous and Next page through the combined list.
  - A removed or renamed icon displays a pin in the shortcut bar and a selected question-mark placeholder in its pin editor. The placeholder identifies the missing filename. The bookmark keeps its name and position. Keep the same filename to replace an icon.

- Use a simple silhouette with a `viewBox`.
  - Kobalt preserves its proportions, renders it with the same LunaSVG library on each host, and uses its transparency as a mask in the current theme's ink color. Source colors become one color.
  - Transparent holes remain transparent; a white filled background becomes solid ink, so remove it before copying the file. Each file must be at most 16 KiB.
  - Filenames must start with an ASCII letter or digit, contain only ASCII letters, digits, underscores or hyphens, and end in lowercase `.svg`. The basename permits at most 44 characters. Up to 120 valid icons are available, sorted by filename.
  - Keep the folder small: discovery stops after 256 entries.

- Supported elements are `path`, `rect`, `circle`, `ellipse`, `line`, `polyline`, `polygon` and groups, with basic fill, stroke, opacity and transforms.
  - Titles, descriptions, classes, accessibility labels and data attributes are ignored. Expand text and references into paths before copying. CSS and `style` attributes, gradients, clipping, masks, filters, images, references, scripts, animation and XML document types are rejected.
  - The limits are eight nested elements, 128 elements, 4,096 characters per attribute and coordinate magnitudes up to 10,000. The `viewBox` width and height must be from 1 to 4,096. Each icon permits at most eight transform operations.
  - Before rasterization, the renderer's transformed ink bounds must be finite and within 4,096 pixels of the origin at the fixed 96-pixel render size. Empty and invalid files are skipped without changing the originals. The picker reports skipped files or a truncated folder.
  - This subset is intended for small icons, not general SVG artwork.

### Bookmark actions

- Tap the top-right area of the page, covering the rightmost 33% of its width and 120 pixels in height at 300 ppi, to bookmark the page or to remove its bookmark. A link in that corner still opens.
- A bookmarked page shows a ribbon at the top: with a header it hangs just left of the top-right slot and runs a little past the strip, and without a header it is shorter and stays in the top margin.
  - A page keeps at most one ordinary bookmark. Pinned bookmarks retain separate destinations after reflow. With no saved bookmarks, the first tap is available as soon as the reading page is ready after opening or reflow.
  - When a layout change puts several ordinary bookmarks on one page, they become one at the page start with the oldest date.
- The bookmark icon next to the menu button opens Bookmarks, sorted by page: each row shows the chapter, the page, the page's first words and the date.
  - Screen readers announce the page, the chapter or pin name, and a short excerpt for each bookmark or annotation. The row's action button also announces the passage.
  - Opening Bookmarks while a bookmark is saving keeps the list open and shows the new row when saving finishes. Saving a bookmark does not change the selected Annotations tab. An open Bookmarks list updates its page numbers when a layout change finishes.
- A row goes to its page and saves the passage you left, as an internal link does.
  - Open the reading menu to return to that passage. Bookmark and link jumps share the same return history, so repeated returns retrace multiple jumps. Completed jumps refresh the reading text, header, footer and bookmark ribbon together without a page-turn animation.
  - Page turns and layout changes keep the saved passages. A failed jump or a jump to the current position adds no return entry.
- The bookmark button at the top right of the Bookmarks tab offers Bookmark current page or Remove bookmarked page. Each row's … menu offers removal with confirmation. Removing a pinned bookmark also removes its shortcut. Removing it from the page corner or the current-page action asks for the same confirmation. Cancel keeps the bookmark and shortcut.
- Opening a bottom navigation tab closes Bookmarks and opens that tab. Annotations comes before Bookmarks in the shared menu. The menu opens Annotations when the book has saved highlights, and Bookmarks otherwise, including when both lists are empty. Both headings use the same Kobalt typeface as Chapters.
- The Text tab shows Aa in the selected reading font, including pending font changes. Opening Bookmarks from Settings, Text or Layout applies the pending changes first. If saving settings fails, the current panel stays open and shows the error.

- The Bookmarks toolbar shows a bookmark with a plus to add the current page, or a crossed-out bookmark to remove it. List rows, their trailing action buttons and popup-menu actions use gray press feedback. Selected rows and tabs keep their active styling.
- Open a bookmark row’s … menu and choose Pin bookmark to give it a name and icon.
  - The icon picker offers four pages of twelve choices, in three rows of four. A screen shorter than 3.4 inches, such as a phone in landscape, shows one row of six per page, on eight pages.
  - When the on-screen keyboard leaves too little room, the editor hides its title and keeps the name field and its buttons. Previous and Next change the icon page without changing the current selection. Editing a pin opens the page containing its icon.
  - Tap the name field to show the keyboard. Done returns to the icon choices; Save stores the name and icon.
- Save adds it to a floating capsule centered vertically near the right edge between the toolbars.
  - Shortcuts stack vertically, with one outer border and short dividers between icons. The capsule has rounded ends and a gap from the screen edge. It grows to fit the shortcuts.
  - When the shortcuts cannot stack at a usable size, as on a phone in landscape, they use separate capsules with a gap between them. The columns have nearly equal numbers of shortcuts and are centered between the toolbars.
  - The gap shows the reading page.
- The bar appears when the reading menu is open and no panel is open.
  - Stats also shows shortcuts in the space above its chart. On short screens they use one column beside Stats instead. Both layouts show as many pins as fit at the usual button size, in book order.
  - Open Bookmarks to reach any pins that do not fit. This also applies when Stats is the default menu view.
- Each book permits up to eight pins. Pinned rows show a pin icon beside the name. The shortcut tray uses each bookmark’s assigned icon and orders shortcuts by their position in the book.
- Tap a shortcut to jump to its saved passage. Moving between pinned shortcuts keeps the return destination at the reading location before the first shortcut. Return takes you back there; the next shortcut starts a new return point. Ordinary bookmark and link jumps keep their normal return history.
- For pinned bookmarks, the first action is Modify pinned bookmark. Use it to rename the bookmark or change its icon. Choose Unpin bookmark in the same menu to remove its shortcut and keep the bookmark. Choose Remove this bookmark to delete it after confirmation.

- Bookmarks are Kobalt's own and separate from Nickel's.
  - Each book has a folder in `.adds/kobalt/annotations/`: `kobo-<id>` for existing UUID book IDs, `kobo-id-<hash>` for other server IDs, and `file-<fingerprint>` for sideloaded books. The server-ID hash preserves case and keeps arbitrary ID characters out of folder names.
  - A moved or renamed sideloaded file keeps its bookmarks through the content fingerprint. Existing UUID folders stay unchanged. `book.json` records the book's identity and title.
  - When a sideloaded file changes, its bookmarks come from the most recently saved folder for that path and move to the new folder on the next save.
  - If saved bookmarks cannot be read, Kobalt keeps the file intact and disables bookmark edits until you fix the file and reopen the book. Opening Bookmarks or tapping the bookmark corner shows an error.
  - If a bookmark save fails, Kobalt shows an error and keeps the previous list and ribbon state. This also applies to automatic merges after a layout change and saves that exceed the 1 MiB storage limit.

- Changed-file bookmark lookup keeps a disposable index in `annotations/.index/paths.json`, limited to 4 MiB and 10,000 book folders.
  - The first lookup in an existing library reads the book metadata to build it. Later lookups check the folder inventory and metadata file stamps, then read only matching books. Missing, invalid or stale indexes are rebuilt.
  - Deleting the index does not delete bookmarks.

## Annotations

- Bookmarks & annotations opens the shared list.
  - Book order is the default and orders annotations by their exact source position, including passages on the same page. The Sort button in the list header opens the shared choice dialog. Newest first and Oldest first order both tabs by creation time.
  - The choice is saved across books and restarts. Lists open on their first page in the selected order. Both lists use smaller chapter headings, a gap before the excerpt and up to two excerpt lines with extra line spacing.
  - Annotation excerpts use the saved color and shading, solid underline or dotted underline. Bookmark excerpts stay plain. Both tabs show the local creation date and time.

- Reading resolves only the saved highlights visible on the current page.
  - Opening the annotations list resolves the saved passages' page numbers and book order when needed. The list shows Finding pages… while that work runs, with the tabs and close button still available. Page turns keep resolved locations for the current layout.
  - Reflow invalidates those locations; recoloring or restyling a saved passage preserves them.

- Hold a word for half a second to select that word. Hold detection uses input timestamps when rendering delays delivery. Cancelling a contact cancels its pending hold, including in the library and font picker.
- In normal reading, releasing it opens dictionary lookup. The selection has an outline and adjustment handles. Moving during the initial hold keeps the original word.
- After release, drag either handle to select a phrase. Releasing a handle looks up the complete phrase in the selected dictionary, with line breaks and repeated spaces treated as one space. Lookup handles stay on the current page and do not enable highlight mode or save an annotation. Tap inside the selection for Dictionary and applicable link actions.
- Normal selection menus offer dictionary lookup, applicable link actions and Clear selection, without annotation tools.
  - Closing the dictionary with X or a tap outside the selection clears the entire lookup selection and its handles when highlight mode is off. The outside tap does not turn a page or open the menu.
  - Closing the dictionary requests a full screen refresh.
- Enable highlight mode from the reading menu before selecting text to annotate.
  - Highlight mode also allows held selection and handle dragging across pages. Saving or clearing a selection keeps that mode active until you turn it off. Ordinary taps keep their page and menu actions. Ordinary swipes keep turning pages.
  - A short tap on a saved highlight does not edit it.

- Open the reading menu and tap Annotate beside Bookmarks to toggle highlight mode.
  - Enabling highlight mode closes the reading menu. Reopening the menu still uses the configured Stats or Navigate default view while highlight mode is enabled. The button stays selected whenever the menu is open while the mode is active.
  - Tap it again to leave highlight mode. Both changes close the reading menu and briefly show a centered indicator in the same style as brightness and warmth feedback.
  - The indicator shows On or Off with the annotation icon; Off adds a slash through the icon. It disappears after 850 milliseconds and does not intercept page input. There is no persistent instruction panel over the page.
  - In highlight mode, with only the reading toolbar open, long press exposed text to close the menu and start selecting in the same gesture. Keep dragging to extend the selection.
  - A brief central two-finger tap on Kobo, or a right-click on Mac, also enters the mode while reading.

- In highlight mode, hold a word to start selecting, then drag to extend. Hold a saved highlight to change it.
- Releasing a new selection, of one word or several, opens the selection menu.
  - The menu appears only after the finger lifts, never while it moves. Adjusting a handle closes the menu. It reopens 1.5 seconds after the finger lifts, unless a handle is pressed again first, after one full cleanup refresh of the screen.
  - Tap inside the outlined selection to open it sooner.
- Inter-word spaces select the preceding word.
  - Margins and paragraph gaps do not start selections. Handles drag immediately, in either direction and across each other. They remain usable while the mark choices are open; starting a grip drag closes the choices and keeps the selection.
  - Expanding requires dragging past the far edge of the added word. At a line boundary, dragging into the adjacent margin includes the first or last word. Off-page endpoints have no handle.
- Clear selection discards the draft. Tapping outside the menu closes it and keeps the draft. Swipes and hardware page buttons still turn pages.
- A highlight can cross pages within a section. Automatic handle dragging stops at EPUB spine sections, semantic sections and table-of-contents chapter boundaries. At those boundaries it selects the first or last word on the current page and ends the drag. Ordinary paragraph endings do not block continuation.
- In highlight mode, drag either handle beyond the first or last word to turn to the previous or next page and continue selecting.
  - Each cross-page extension ends the drag at the last word of the previous page or the first word of the next page. Holding or moving the same press cannot turn another page or move that boundary.
  - Release and grab a handle again to continue adjusting the selection. An extension cannot cross another saved highlight.

- The start and end tokens are included, in either order.
  - Ending on a sentence's final word includes its terminal punctuation when the whole final sentence is selected. A sentence can end in a number, as in “Published in 1984.” A fragment keeps the exact word ending.
  - Kobalt omits automatic punctuation when an ellipsis, abbreviation, decimal or limited context makes the sentence boundary uncertain. A complete first sentence can include its attached opening quote or bracket when the matching closing mark is included.
  - A closing quote or bracket is included only when its opening mark is selected in that sentence. Automatic punctuation keeps pairs balanced. Moving a boundary recalculates punctuation from the chosen words. Saved highlights retain those chosen boundaries for later adjustment.
  - To choose an exact punctuation ending, tap the punctuation directly. Older saved highlights keep their exact boundaries until you choose a new boundary.

- The menu has no section headings. It uses a fine grid, with a thin divider between rows.
- Color swatches come first when available, as unlabeled circles centered in their row; their names are their accessible names.
  - Shaded, Solid and Dotted samples follow in one row, without labels; their names are their accessible names. Each style stays on paper, and a small checkmark beside the sample marks the selected choice. The mark choices remain available on grayscale displays.
- Actions stay below the choices: Mark text, Add note and Clear selection for a draft, Apply changes, Add note or Edit note, and Discard while a saved highlight is adjusted, or Apply changes, Add note, Adjust and Delete for a saved highlight without a note.
  - Every menu keeps one order and leaves out what does not apply: save, note, color, adjust, look up, follow link, then the action that discards. Add note or Edit note while adjusting applies the new range first, then opens the note.
  - The dictionary action is called Look up in every menu. Four actions share one row when their labels fit, and form two rows otherwise. There is no Dismiss button; tap outside the menu or press Escape to close it.
  - Action cells place their icons above full labels at the same text size as the style samples. The selected style is checked.
- Color displays offer Yellow, Green, Blue, Pink and Gray above the style samples, with a divider below the palette. Changing a choice updates the samples without saving. Mark text saves both choices and dismisses the menu. New highlights use the last saved style and color, initially Shaded and Yellow. Underlines and dotted underlines use the chosen color.
- Grayscale devices omit the color picker, draw shading in gray and use black or white underlines. They preserve stored colors. Outlines mark only the draft being adjusted.
- A long press on saved text opens that highlight’s actions only in highlight mode. Outside highlight mode, it looks up the selected word. In highlight mode, selection handles stop before another saved highlight, including when dragged beyond it. Adjust changes the saved passage without allowing it to overlap a different highlight.
- Tapping outside the menu, including on the selected text, dismisses it without turning a page or opening the reading menu. In annotation mode, the draft and its choices remain available. Outside annotation mode, dismissal clears the word selection. A failed save keeps the draft for another attempt. Annotation mode stays active after saving.

- Saved highlights are local to Kobalt.
- Open Bookmarks and choose Annotations to see each saved passage once, with its excerpt and page or page range.
  - A highlight with a note shows up to two lines of it below the excerpt, after a note icon. Tap a row to go to its start. The row's remove button asks before deleting.
  - Bookmarks and Annotations use the interface font for labels and the selected reading font for excerpts, at the same size as Search results.
- In annotation mode, hold a saved passage for its actions. Without a note, the menu shows the appearance pickers and Apply changes, Add note, Adjust and Delete. With a note, the menu shows up to three lines of the note and one row of actions: Edit note, Color, Adjust and Delete. Grayscale displays name the second action Style.
- Color opens the pickers in the same menu, with Apply changes and Discard. Discard returns to the note and abandons unsaved appearance changes. Apply changes saves the chosen color and style together and dismisses the menu. Closing the menu abandons unsaved appearance changes. A failed change keeps the previous saved appearance.
- Adjust explicitly enters annotation mode and loads the saved range as an outlined draft with handles. Apply changes saves the adjusted range with the same ID. Close or Discard drops the adjustment and keeps the saved original.
- New highlights and range edits cannot overlap another saved highlight. A rejected save keeps the selection and saved passages unchanged. Legacy overlapping highlights remain readable. Switching away from a failed complete draft offers save and discard actions; tapping outside keeps the draft.

- Add note on a draft saves the highlight with the chosen color and style, then opens the note editor.
  - Add note and Edit note on a saved highlight open the same editor. The editor is a centered card with the patterned backdrop. Its heading reads Add note or Edit note.
  - Below it, the chapter and up to three lines of the passage appear in the reading font with the highlight's marking. A longer passage ends with an ellipsis. The text box shows four lines and opens with the keyboard.
  - On Kobo it uses Nickel's keyboard. When the keyboard leaves too little room, the card hides the heading, then the passage, then shrinks the text box to two lines. They return when the keyboard closes. Done closes the keyboard.
  - Save stores the note and closes the card; Cancel or a tap outside closes it without saving. Cancel after Add note on a draft keeps the saved highlight without a note.
  - Saving an empty note, or one of only spaces and line breaks, removes the note. A note can hold 65,536 UTF-16 code units. A longer note, or a failed save, shows an error below the text box and keeps the text.
  - Notes are plain text. Adjust keeps the note when it changes the saved range.

## Quick annotation mode

- Settings > Controls > Annotations > Highlighting chooses how highlighting starts.
  - Toggleable annotation mode is the default and works as described above. Quick annotation mode hides the Annotate button and the On and Off indicator, and ignores the two-finger tap, right-click and navigation-bar Annotate shortcuts.
  - In Quick annotation mode, hold a word to start a highlight. Keep holding and drag to extend it, also across pages, as in highlight mode.
  - Releasing opens the menu with the color and style choices and Mark text, Add note, Look up and Clear selection. When the selection touches a link, Follow link is added; five actions use rows of three.
  - The selection keeps its black outline, as in highlight mode. Holding a saved highlight opens its actions. Look up leaves annotation mode, keeps the selection and opens the dictionary, as an ordinary lookup does.
  - The other actions and a tap outside the menu end the selection.
  - Annotation mode lasts only as long as one selection, its menu, the note editor or the delete confirmation, so link taps, edge light gestures and other reading input work as usual between selections.
  - Changing the setting while a selection is open ends it.

- Selection handles are dots 36 pixels across at 300 ppi, about 3 mm.
  - A held handle grows to 48 pixels until the finger lifts. A drag starts within 48 pixels of a handle's center, about 8 mm across, also while the selection menu is open.
  - Where the menu card covers part of that area, only the drawn dot passes presses to the handle, so the card's controls stay usable.

- Ordinary reading taps keep their page and menu actions. A tap on ordinary book text does not open word actions. Link taps follow the Tappable links setting. Long press still opens dictionary or link options deliberately.

## Annotation export

- Sort sits at the left edge of the header, before the tabs. The action cell, just before the close cell at the far right, follows the active tab. Bookmarks shows a bookmark icon and offers Bookmark current page or Remove bookmarked page. Annotations shows an export-arrow icon. Both buttons stay visible when their list is empty.
- The menu opens a centered card with the same patterned backdrop as Library. It covers the full reader, including the status bar, tabs and bottom controls. Tapping outside the card dismisses it without activating the controls underneath.
- On Annotations, the card has Export as Markdown… and Export as HTML… actions.
  - A shared description above the two actions explains ZIP compression and extraction. The card uses the standard action menu with single-line icon rows. A pressed row uses the shared gray background and keeps its normal text and icon colors until release.
  - Export is disabled when the annotation list is empty or unavailable. Choose one format from this card. Each format action has a ZIP-file icon.
- It exports every saved highlight, including its chapter, creation timestamp and any saved note, in a ZIP containing either UTF-8 `annotations.md` or `annotations.html`, according to the selected format.
  - Each document includes the book title and author. Its suggested filename uses a shortened book title, a book identity hash and a `-markdown.zip` or `-html.zip` suffix. The two formats have separate destinations.
- Confirm Export ZIP before exporting.
  - The confirmation identifies the book and selected format. On Kobo, it also shows the destination under `Kobalt/Annotations/` at the root of the drive. If an export exists, the dialog asks Replace annotation export? and requires Replace ZIP. Cancelling preserves the existing file.
  - Kobalt checks again before writing if a file appeared while the first confirmation was open.
- On Mac and Android, confirming Export ZIP opens the system save picker, with its overwrite confirmation enabled.
  - Choose the filename and location there. Cancelling the picker creates no export. A failed write shows an error. Local file exports use atomic replacement; Android document providers control how their files are written. Export does not change annotations or Nickel settings.
- The HTML opens in book order and includes a Sort by list for Book order, Newest first and Oldest first. Date sorts order all annotations together and show each annotation’s chapter. Search filters passages, notes and chapter labels.
  - HTML uses compact `#1`, `#2` annotation links. The numbers stay fixed when the order changes. Exported passages use the saved highlight color and marker or underline style. Libron is embedded for the passages, so the HTML does not need a font download.
- Timestamps include the time; HTML uses the browser’s locale and timezone, and Markdown uses UTC. Exports do not include page numbers.
- The checked Show annotation styling option below search hides or restores marker fills and underlines.
  - It keeps passages, notes and the embedded font visible, and applies to printing.

## Notebook pages (fork)

- Every highlight can have a handwritten notebook page. Choose Notebook in the selection menu of a new highlight, or in the actions of a saved one. A new highlight is saved first.
- In ordinary reading, a short tap on a highlight that has a notebook page opens that page. Highlights without a page keep the usual tap behavior.
- The notebook covers the reader. Its toolbar has Done, previous and next page, the page number, Pen or Eraser, Undo and Clear. The page buttons also turn notebook pages. Clear can be undone.
- Strokes use the direct waveform while the pen moves, like a selection drag, and one text-waveform cleanup about 1.5 seconds after the pen lifts. The line is smoothed with a low-pass filter and quadratic curves between midpoints. Real pen pressure, when the driver reports it, varies the width. Only the first contact draws; a second finger or palm is ignored.
- A page that opened for a highlight without one is linked when something is drawn on it. The highlight's passage appears above its page, on screen only, as does the 5 mm dot grid.
- Pages are saved on the writer thread 4 seconds after the last stroke, on page changes and on Done. They live in `.adds/kobalt/notebooks/<book folder>/` as `page-001.png` and on, with transparent paper, and `links.json` maps highlight IDs to page numbers. The book folder name matches the annotations folder.
- Deleting a highlight keeps its page.

### Kobo's pen layer (firmware 4)

This applies only where Nickel's notebook app cannot be opened. It was tried on a Kobo Sage and drew late and without smoothing.

- The notebook page draws with Nickel's own pen layer (`SketchNote`, the one Nickel uses for stylus markup on book pages) instead of Kobalt's strokes. Only the pen draws; fingers work the toolbar, which shows Redo in place of Pen/Eraser.
- The whole notebook of a book is one file, `.adds/kobalt/notebooks/<book>.notebook`: JSON with `pages` (each page's strokes as an SVG document, in order) and `links` (highlight ID to page number).
- Nickel's layer loads strokes only from an SVG file, so the page being opened is first written to `/tmp/kobalt-notebook-page.svg`, which is in memory.
- While the layer is shown, the reader's flush passes Nickel's refresh policy on unchanged.

### The book's own notebook in Kobo's notebook engine (firmware 4)

- On firmware 4, Notebook opens the book's own notebook in the Kobo's notebook engine, the one behind My Notebooks, instead of the page above: its pen, eraser, toolbar and page backgrounds. If the engine cannot be opened, the page above is used for the rest of the session.
- The notebook is one file that Kobalt keeps, `.adds/kobalt/notebooks/<book folder>/book.nebo`, a basic (free ink, paged) notebook. Kobalt builds the description Nickel needs to open it, the way Nickel's new-notebook dialog does. Nickel's editor creates the file the first time and saves it; Kobalt only makes the folder, on the writer thread. When Nickel's editor saves a notebook it also saves its description to Nickel's library, so My Notebooks lists the notebook under the book's title after it was first closed.
- Every highlight with notes has its own page. A highlight without one gets the notebook's empty last page. Nickel's editor adds an empty page behind the last page as soon as that page has ink, so a page that is no longer the last one was written on, and it then belongs to its highlight. While it is still the last page it waits for that highlight (`pending`), and Notebook or a tap on the same highlight opens it again; another highlight without a page takes it over only when Nickel's editor shows that it is empty. If the editor cannot show that (it stays busy, or this firmware's editor is laid out differently), the last page is taken only when it belongs to no highlight and waits for none; otherwise the new highlight gets no page that time.
- `pages.json` beside the notebook lists, for each page in order, the highlight it belongs to (an empty string for a page without one), and in `pending` the highlight the last page waits for. Pages that are deleted or added inside the notebook are followed while it is open, so the list stays in step. If Nickel's editor shows no page (the notebook could not be read), the list is left as it was, and so is a `pages.json` that cannot be read or comes from a newer version. A notebook with fewer pages than the list was changed in My Notebooks: its pages can no longer be matched, so they lose their highlights and only the page just opened gets one. The list is saved even when the reader closes while the notebook is open. Deleting a highlight keeps its page.
- The highlight's passage is shown in a band over the top of its page, at most 280 characters, drawn by Kobalt. The band is not part of the notebook file, and pen and finger do nothing on it. In a basic notebook Nickel's toolbar is not part of the screen's layout: it lies over the top of the page while the menu is open, and the button that opens the menu sits in the top right corner while it is closed. The band lies directly above the editor and below those two, so the open toolbar covers it, and its text stays clear of the button.
- The book stays open. Kobalt hides the reader the way it does for Nickel's browser, Nickel puts the notebook screen on its page stack, and the reader returns to the same page when Nickel takes that screen off the stack. Another page of Nickel's over the notebook (its choice of page backgrounds) does not count as closing. Nickel keeps the page each notebook was left on (`PartIndex`) in `Kobo eReader.conf`, group `IInkSettings`, and caches it once read; Kobalt sets it through Nickel's own notebook settings before the notebook opens, so the notebook opens on the highlight's page. When Nickel first asks to download its notebook files, the reader waits while that dialog is open, and it returns when Nickel shows a page other than the notebook.
- A short tap on a highlight that has a page opens that page.
