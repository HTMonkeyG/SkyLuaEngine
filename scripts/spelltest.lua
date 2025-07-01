-- not networked
local e = game:eventBarn():AddEventByMetaName("GrantSpell")
e:networked(true)
e:duration(1)
e:buffName("glow_rainbow")
e:Start()
