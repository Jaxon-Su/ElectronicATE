#pragma once
#include <QWidget>
class TestSequenceResultsModel;

class TestSequenceResultDetails : public QWidget {
public:
    explicit TestSequenceResultDetails(TestSequenceResultsModel *model, QWidget *parent = nullptr);
};
