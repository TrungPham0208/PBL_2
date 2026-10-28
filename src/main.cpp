#include <bits/stdc++.h>

#include <QApplication>
#include "ui/MainWindow.h"

using namespace std;

// Milestone 11: Supporting/boilerplate — Qt dashboard and topology controls
// Application entry point for the PBL2 Network Simulator.
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    pbl::ui::MainWindow window;
    window.show();

    return app.exec();
}
