/******************************************************************************
    Copyright (C) 2016 by Hugh Bailey <obs.jim@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/


#include <util/dstr.hpp>
#include "BroardcastBase.hpp"

using namespace std;


void BroardcastBase::TransitionStopped()
{
}

void BroardcastBase::InitTransition(obs_source_t *transition)
{
	auto onTransitionStop = [](void *data, calldata_t*) {
		BroardcastBase *window = (BroardcastBase*)data;
		window->TransitionStopped();
	};

	signal_handler_t *handler = obs_source_get_signal_handler(transition);
	signal_handler_connect(handler, "transition_video_stop",
		onTransitionStop, this);
}

void BroardcastBase::InitDefaultTransitions()
{
	std::vector<OBSSource> transitions;
	size_t idx = 0;
	const char *id;

	/* automatically add transitions that have no configuration (things
	 * such as cut/fade/etc) */
	while (obs_enum_transition_types(idx++, &id)) {
		if (!obs_is_source_configurable(id)) {
			const char *name = obs_source_get_display_name(id);

			obs_source_t *tr = obs_source_create_private(
					id, name, NULL);
			InitTransition(tr);
			transitions.emplace_back(tr);

			if (strcmp(id, "fade_transition") == 0)
				fadeTransition = tr;

			obs_source_release(tr);
		}
	}
}




void BroardcastBase::TransitionToScene(obs_scene_t *scene, bool force)
{
	obs_source_t *source = obs_scene_get_source(scene);
	TransitionToScene(source, force);
}

void BroardcastBase::TransitionToScene(obs_source_t *source, bool force)
{
	obs_scene_t *scene = obs_scene_from_source(source);
	if (!scene)
		return;

	obs_source_t *transition = obs_get_output_source(0);

	if (force)
		obs_transition_set(transition, source);
	else
		obs_transition_start(transition, OBS_TRANSITION_MODE_AUTO,
				300, source);

	obs_source_release(transition);
}


void BroardcastBase::SetTransition(obs_source_t *transition)
{
	obs_source_t *oldTransition = obs_get_output_source(0);

	if (oldTransition && transition) {
		obs_transition_swap_begin(transition, oldTransition);
		obs_set_output_source(0, transition);
		obs_transition_swap_end(transition, oldTransition);
	} else {
		obs_set_output_source(0, transition);
	}

	if (oldTransition)
		obs_source_release(oldTransition);

	bool fixed = transition ? obs_transition_fixed(transition) : false;

	bool configurable = obs_source_configurable(transition);
}

void BroardcastBase::SetCurrentScene(obs_source_t *scene, bool force)
{
	if (!IsPreviewProgramMode()) {
		TransitionToScene(scene, force);
	}
}

void BroardcastBase::SetCurrentScene(obs_scene_t *scene, bool force)
{
	obs_source_t *source = obs_scene_get_source(scene);
	SetCurrentScene(source, force);
}

void BroardcastBase::InitOBSTransition()
{
	InitDefaultTransitions();

	if (fadeTransition){
		SetTransition(fadeTransition);
	}
}