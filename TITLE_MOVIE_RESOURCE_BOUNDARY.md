# AC6 title-movie resource boundary

Date: 2026-07-15

## Closed media-pack facts

The local retail input contains
`game-files/moviepack.bin` (348,307,456 bytes).  Its leading GUID is the
Microsoft ASF header object, and `file` identifies it as Microsoft ASF.

The XEX function exported at `0x821D7ED8` is an independent media-resource
initializer.  It:

1. resolves entry `9` from the DPL/table rooted at `0x829DDE68` through
   `0x82234DD0`;
2. copies the explicit path `game:\\moviepack.bin` into its runtime object;
3. stores the entry-9 result at that object's `+0x118` field.

This closes an available native input for future media decoding.  It does not
identify a title clip, a menu item, or a playback order within the pack.

## Why no title-movie playback or selector was added

`CModeTaskTitleMovie` is independently typed by RTTI, but its recovered
constructor/update range (`0x821B8EF0..0x821B91D0`) has no serialized media
path, ASF byte range, clip index, stream identifier, or call to the
`0x821D7ED8` media initializer.  Instead it asks a generic service at
`0x82671308`, virtual slot `+0x14`, for auxiliary type value `2`, stores the
dynamic result at task `+0x270`, and controls it only through virtual slots.

The local XEX does contain RTTI names `CNuMoviePlayer` and
`ACE6::CAce6MoviePlayer`, but the available static references do not connect
either RTTI object to service type `2`, the title task's `+0x270` object, or a
specific byte range in `moviepack.bin`.  The service is also used by sibling
mode-task code.  Assigning auxiliary type `2` to either movie-player RTTI
type would therefore be a guess.

The type-name strings are an especially weak lead in this image: an exact
32-bit scan finds no complete-object locator/type-descriptor reference to
`0x826EB764` (`CNuMoviePlayer`) or `0x826EB784`
(`ACE6::CAce6MoviePlayer`).  In contrast, the already typed title task's
descriptor `0x826EA89C` resolves through complete-object locators at
`0x82077720` and `0x8207770C` to its vtables at `0x82065634` and
`0x82065684`.  The movie-player names alone therefore do not yield a vtable,
constructor, or service-factory case to trace.

The factory pointer at `0x82671308` is also zero in the static XEX image.
Its concrete vtable and the type-`2` allocation switch are injected at
runtime, so a static call at title construction can establish only its slot
and arguments, not the returned object's class or media cue.

There is likewise no recovered title menu/table mapping an input to a movie
clip, campaign selector, mission identifier, or Scene group.  The known title
inputs only advance/skip the opaque auxiliary object and then publish the
generic mode-flow tuple `{1,3}`.

Consequently, a native player could at most expose the whole media pack as an
unmapped external asset.  Selecting an arbitrary ASF stream, calling it the
title movie, or mapping title input to a campaign route would fabricate retail
semantics and is intentionally not implemented.

## Required next edge

Before adding a title-media control, recover one of:

1. the concrete object returned by service type `2`, including a media cue or
   decoder request with file/offset/stream fields; or
2. a menu-resource table that maps a title input to a concrete movie-pack
   segment or campaign selector.

Evidence retained in
`reports/title-moviepack-resource-flow.log`,
`reports/title-movie-player-rtti-service-refs.log`, and
`reports/title-movie-service-sibling-flow.log`.  The RTTI and runtime-factory
negative checks are in `reports/title-movie-player-rtti-u32.log`,
`reports/title-rtti-vtable-layout.log`, `reports/title-rtti-col-layout.log`,
and `reports/title-movie-service-global.log`.
