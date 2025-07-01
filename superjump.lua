-- doesn't work if on the ground, jump then quickly run this
local e = game:eventBarn():AddEventByMetaName("SetAvatarMinYSpeed")
e:forceTakeoff(false)
e:minYSpeed(500)
e:fireOnce(true)
e:Start()
