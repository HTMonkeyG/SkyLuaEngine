-- doesn't work (cannot figure out what is expected inside the clump)
-- we have a hook in the main game loop, so we could implement our own thread scheduler + wait/delay function if we wanted to
-- it's just frustrating, it's probably something like this though

local fwevent = game:eventBarn():AddEventByMetaName("SocialFireworks")
local clump = game:clumpBarn():CreateClumpWithSize(1)
clump:SetAt(0, fwevent.Start)

local delay = game:eventBarn():AddEventByMetaName("Delay")
delay:staggered(false)
delay:useFrames(false)
delay:waitForBeat(false)
delay:delay(1)
delay:events(clump)
delay:Start()
