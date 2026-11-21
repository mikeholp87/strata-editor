#pragma once
#include "document.h"
bool writeExport(const Document &document, const QString &path, bool pdf, QString *error);
