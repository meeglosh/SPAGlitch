# Spa artwork

Original artwork generated with the built-in image generation tool for the
SPAGlitch redesign. The user's reference images guided composition and the
electrocution concept; neither reference is bundled in the product.

- `spa-calm.png`: overhead spa photograph, 1536 × 1024.
- `spa-electric-1.png` through `spa-electric-5.png`: five shocked expressions with an electrical blast background, aligned to the same overhead composition.

All six images are embedded with JUCE BinaryData, so installed plugins require no
external image paths. Audio switches instantly between fully opaque calm and zapped states, with a
small silhouette-clipped twitch during playback. Motion can be disabled independently of the audio-triggered image switch.

The electric image was revised at the user's request to add a shocked,
asymmetrical open-mouth expression. Final expression-edit prompt:

Edit this exact electrified spa photograph. Change ONLY her facial expression in the mouth/cheek/jaw region: the woman is being comically zapped and should look unmistakably SHOCKED instead of serene. Her mouth opens into a crooked involuntary gasp/grimace, lips pulled asymmetrically with one corner yanked outward, upper teeth visible, jaw tensed and slightly dropped, cheeks visibly contracting. Playful cartoon electrocution expression translated into realistic photographic facial muscles. Not a smile, not a relaxed pout. Keep her recognizable and plausible, not monstrous. Adjust the x-ray teeth/jaw overlay to follow this contorted open mouth accurately, without a duplicate serene mouth underneath. Preserve EXACT image dimensions, overhead camera, framing, head orientation and head position, cucumber slices precisely in place covering eyes, towel, shoulders, neck, background props, mask, cyan x-ray lighting and electrical arcs. Same body silhouette and pose for crossfading with the calm photograph. No gore, no injury, no text. The expression change must be obvious even at small instrument UI size.

## Calm image prompt

Use case: photorealistic-natural. Asset type: central photograph for SPAGlitch audio instrument UI. Create a NEW original editorial spa photograph, landscape 3:2 composition. Straight overhead camera, woman's head at top, chin toward bottom, close crop from towel-wrapped crown through shoulders/upper chest, centered face taking much of frame, like an intimate overhead facial treatment portrait. Adult woman lying flat on a white massage table, eyes covered by two fresh cucumber slices, pale sage green clay facial mask, white terry towel wrapped around hair, modest white towel covering chest. Relaxed closed mouth with slight peaceful smile. Face is perfectly frontal and centered, shoulders horizontal. Subtle folded towels, small ceramic bowl of face mask and cucumber slices at outer edges. Real pores, beautiful natural skin, rich tactile linen, warm directional spa lighting, muted eucalyptus green and creamy warm whites. Premium photographic realism, not illustration. No text, no logos, no watermarks, no interface or knobs. This will be edited into an electrically jolted x-ray state with exactly matching geometry, so keep a clear symmetrical frontal pose and clean legible silhouette.

## Electric edit prompt

Use case: identity-preserve. Edit target: the supplied spa photograph. Create its electrically activated animation frame. Preserve EXACT composition, dimensions, camera, head shape, neck, shoulders, towel, cucumber positions, every background prop and photograph registration. Same adult woman lying completely flat in EXACT same position, not a new pose. Transform her face neck and upper chest into a playful luminous cyan x-ray showing skull, teeth, cervical spine and clavicles registered precisely INSIDE her existing silhouette. A semi-transparent photographic skin/mask layer remains so this visibly is the SAME photographed woman with cucumbers on eyes, not a separate cartoon skeleton. Cucumbers remain plainly recognizable in same location. Bright thin cyan-white branching electrical arcs hug her silhouette across forehead, jaw and shoulders. Funny cartoon electrocution energy rendered as a photographic VFX overlay, no injury, no gore, no horror, no burnt skin, no raised hands, no text. Keep the white towels and spa props unchanged and warm-toned; cool radiographic light restricted mostly to her body. Her body position and outer silhouette MUST stay aligned to original for crossfading. No full-frame white flash.

## Blast background and final variations

The five final electric frames replace the spa surroundings with a radial electrical blast. They are embedded alongside the calm image. A shuffled bag presents all five expressions before reshuffling, with no immediate repeat; sustained audio changes expression every 300–500 ms. Motion off holds the expression during sustained audio and disables twitching.

The background is animated in JUCE: two crossfaded outward image drifts, five
independently regenerating branched arcs with fading cyan halos, and 32 outward
sparks. The subject is clipped out of these layers and receives small irregular
jolts. All animation runs on the UI thread at 30 Hz and follows held output peaks;
Motion off disables background motion as well. No extra generated artwork is used.

### blastPrompt

Edit this exact electrified woman image. KEEP the woman herself EXACTLY aligned: same shocked crooked open-mouth expression, head position, face geometry, cucumbers on eyes, white hair towel, shoulders, cyan x-ray skull/spine/ribs, body outline and modest chest towel. Change the entire BACKGROUND around her into a spectacular cartoon-electrocution BLAST, photo-realistic VFX style. Remove ALL visible massage table, folded towels behind her, cucumber bowls, plants, wooden boards and quiet spa props. Instead: deep ink-blue/teal energy void, explosive radial cyan-white electric streaks erupting from behind her head and shoulders, jagged lightning spokes, bright turquoise sparks, a few hot lime and pale golden sparks, turbulent luminous electrical clouds. Strong outward burst lines communicate an enormous jolt; playful exaggerated impact, not fire, not injury. Still the exact same overhead view and crop and position of the woman. Background must feel kinetic and explosive everywhere rather than a room. Clearly visible face and x-ray anatomy remain focal. Not a flat comic sticker, not a literal bomb explosion, no text, no watermarks, no gore. Same image dimensions as input.

### blastVariationPrompt2

Edit this EXACT electric-blast photograph to change ONLY the woman's mouth expression to: mouth shut with teeth clenched visibly, lips stretched taut sideways in startled EEEEK; not smiling, not an open mouth. Keep her head, cucumbers, nose, shoulders, white head/chest towels, cyan x-ray anatomy, lighting and explosive background absolutely identical and in the same position. Same size, same overhead camera, same crop. Correct the x-ray jaw and teeth for the new expression. Playful cartoon electrocution translated into photographic facial muscles. Clearly different expression, no serene smile, no gore, no text, no new pose. This is one frame in a registered five-expression UI animation.

### blastVariationPrompt3

Edit this EXACT electric-blast photograph to change ONLY the woman's mouth expression to: mouth open in a pronounced round vertical O of shock, rounded lips and dropped jaw; not an asymmetrical grimace. Keep her head, cucumbers, nose, shoulders, white head/chest towels, cyan x-ray anatomy, lighting and explosive background absolutely identical and in the same position. Same size, same overhead camera, same crop. Correct the x-ray jaw and teeth for the new expression. Playful cartoon electrocution translated into photographic facial muscles. Clearly different expression, no serene smile, no gore, no text, no new pose. This is one frame in a registered five-expression UI animation.

### blastVariationPrompt4

Edit this EXACT electric-blast photograph to change ONLY the woman's mouth expression to: lips tightly puckered and pulled sharply sideways in an involuntary cheek-clenching electrical twitch, comically crooked closed lips; no smile, no open mouth. Keep her head, cucumbers, nose, shoulders, white head/chest towels, cyan x-ray anatomy, lighting and explosive background absolutely identical and in the same position. Same size, same overhead camera, same crop. Correct the x-ray jaw and teeth for the new expression. Playful cartoon electrocution translated into photographic facial muscles. Clearly different expression, no serene smile, no gore, no text, no new pose. This is one frame in a registered five-expression UI animation.

### blastVariationPrompt5

Edit this EXACT electric-blast photograph to change ONLY the woman's mouth expression to: mouth stretched very wide horizontally in a startled AAAGH, lower lip pulled down and to viewer right, upper teeth exposed, tense cheek and jaw; distinct from a round O or clenched teeth. Keep her head, cucumbers, nose, shoulders, white head/chest towels, cyan x-ray anatomy, lighting and explosive background absolutely identical and in the same position. Same size, same overhead camera, same crop. Correct the x-ray jaw and teeth for the new expression. Playful cartoon electrocution translated into photographic facial muscles. Clearly different expression, no serene smile, no gore, no text, no new pose. This is one frame in a registered five-expression UI animation.

The calm/zapped transition uses a hard cut on each 30 Hz UI update: any held
output peak above 1e-7 selects the fully opaque zap; the first silent update
restores the calm photo. There is no attack fade, release fade, or opacity
tracking of audio volume. Internal blast motion continues during the zap.
