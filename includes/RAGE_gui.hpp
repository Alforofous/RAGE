#pragma once

class RAGE_scene_view;
class RAGE;

class RAGE_gui
{
public:
	RAGE_scene_view	*scene_view;

    RAGE_gui(RAGE *rage);
    ~RAGE_gui();
    void draw(RAGE *rage);
private:
};