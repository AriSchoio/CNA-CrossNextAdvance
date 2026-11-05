# Reading Tools

## Annotate a quote

Open **Apps > Quotes** to browse the latest clippings exported to `/My Clippings.txt`. Open a quote and choose **Add note** or **Edit note**. Enter up to 128 UTF-8 bytes; saving an empty note removes it. Notes, favorites and hidden quotes are stored locally in `/.crosspoint/quotes.bin`. The original clipping export is not rewritten. Existing version-1 quote state remains readable.

## Edit EPUB details

Connect the reader and open **File Transfer** from the reader, then open the Files page on a phone or computer on the same network. Use the book-row details action to edit the title, author, language, publisher, description, subjects and series. Saving replaces only the EPUB archive after staging and validating it; it does not rename the book or move its clipping/read-progress sidecars.

## Look up selected text in Wikipedia

In the reader, start a text selection and choose **Wikipedia** from the selection actions. An exact match opens immediately; otherwise the selected text is placed into the offline Wikipedia search. The matching pack content must be installed on the microSD. This lookup does not use Wi-Fi.

## Share a note to a phone

Open a note, open its options, then choose **Send this note to your phone**. If needed, connect the reader to Wi-Fi. Scan the displayed QR code with a phone on the same network. On the phone page, **Copy note to messages app** copies the current note text so it can be pasted into SMS or another messenger. **Save to the reader** writes edits back to the open note. This is a local copy/paste workflow; the firmware does not send SMS or deliver messages to remote accounts.

## Web Browser

**Apps > Web Browser** opens HTTP(S) addresses and presents extracted page text and ordinary links. See the [browser guide](browser.md) for navigation and the 64 KiB response limit. It does not execute scripts, submit forms, store cookies or display images. The shared HTTPS transport does not verify server certificates; do not enter credentials or private information.

Habits' daily completion rail and progress artwork are described in the [Habits guide](habits.md).
