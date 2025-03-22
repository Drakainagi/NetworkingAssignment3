

#include "Application.h"

#pragma region IGNORE
int main( void )
{
	Application &app = Application::GetInstance();
	app.Init();
	app.Run();
	app.Exit();
}
#pragma endregion