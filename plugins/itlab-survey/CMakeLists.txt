# itlab-survey - khảo sát nhanh trong giờ học (GĐ1b, docs/ITLabControl_Survey_Design.md)

include(BuildVeyonPlugin)

build_veyon_plugin(itlabsurvey
	NAME ItlabSurvey
	SOURCES
	ItlabSurveyPlugin.cpp
	ItlabSurveyPlugin.h
	SurveyQuestion.cpp
	SurveyQuestion.h
	SurveyComposeDialog.cpp
	SurveyComposeDialog.h
	SurveyResultsDialog.cpp
	SurveyResultsDialog.h
	SurveyAnswerWindow.cpp
	SurveyAnswerWindow.h
	itlab-survey.qrc
	)
