local waitTime = 7.5

local t = Def.ActorFrame {
	InitCommand=cmd(x,SCREEN_CENTER_X;y,SCREEN_CENTER_Y);
	OnCommand=cmd(queuecommand,"TweenOn";sleep,waitTime;queuecommand,"TweenOff");
}
t[#t+1] = Def.Quad {
	InitCommand=cmd(zoomto,SCREEN_WIDTH,SCREEN_HEIGHT;diffuse,Color.Black);
	TweenOnCommand=cmd(diffusealpha,1;linear,0.5;diffusealpha,0.8);
	TweenOffCommand=cmd(linear,0.5;diffusealpha,0);
};
t[#t+1] = Def.ActorFrame {
	LoadFont("Common Normal") .. {
		Text=ScreenString("WarningHeader");
		InitCommand=cmd(y,-70;diffuse,Color.Red);
		TweenOnCommand=cmd(diffusealpha,0;zoomx,2;zoomy,0;sleep,0.5;smooth,0.25;zoom,1;diffusealpha,1);
		TweenOffCommand=cmd(linear,0.5;diffusealpha,0);
	};
	LoadFont("Common Normal") .. {
		Text=ScreenString("WarningText");
		InitCommand=cmd(y,10;wrapwidthpixels,SCREEN_WIDTH-48);
		TweenOnCommand=cmd(diffusealpha,0;sleep,0.5125;linear,0.125;diffusealpha,1);
		TweenOffCommand=cmd(linear,0.5;diffusealpha,0);
	};
};

return t