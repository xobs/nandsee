#ifndef NANDSEEWINDOW_H
#define NANDSEEWINDOW_H

#include <QMainWindow>
#include <QModelIndex>
#include <QItemSelectionModel>

#define log2of10 3.32192809488736234787
#define MONTEN	6		      /* Bytes used as Monte Carlo
                     co-ordinates.	This should be no more
                     bits than the mantissa of your
                                         "double" floating point type. */


namespace Ui {
class NandSeeWindow;
}

class EventItemModel;

class HexWindow;
class NandSeeWindow : public QMainWindow
{
	Q_OBJECT
	
public:
	explicit NandSeeWindow(QWidget *parent = 0);
	~NandSeeWindow();
	
public slots:
	void eventSelectionChanged(const QItemSelection &index, const QItemSelection &old);
	void changeLastSelected(const QModelIndex &index, const QModelIndex &old);
	void openHexWindow(const QModelIndex &index);

	void xorPatternChanged(const QString &text);
	void optimizeXor();

	void exportCurrentView();
	void exportCurrentPage();

    void ignoreEvents();
    void unignoreEvents();

    void closeHexWindow(HexWindow *closingWindow);

    void updateAlign(int value);


private:
	Ui::NandSeeWindow *ui;
	EventItemModel *_eventItemModel;
	QItemSelectionModel *_eventItemSelections;
	QByteArray currentData;
	QModelIndex mostRecent;
	QByteArray _xorPattern;
    int _xorPatternSkip;
    int lastAlignAt;

    long ccount[256],	   /* Bins to count occurrences of values */
            totalc; 	   /* Total bytes counted */
    double prob[256];	   /* Probabilities per bin for entropy */


    int mp, sccfirst;
    unsigned int monte[MONTEN];
    long inmont, mcount;
    double cexp, incirc, montex, montey, montepi,
              scc, sccun, sccu0, scclast, scct1, scct2, scct3,
              ent, chisq, datasum;

    double r_ent, r_chisq, r_mean, r_montepicalc, r_scc, r_chip;

	void updateEventDetails();
	void updateHexView();
	void hideLabels();
    void initEntropy();
    void updateEntropy(unsigned char *buf, int bufl);
    void finalizeEntropy();
    double poz(double z);
    double pochisq(double ax, int df);
};

#endif // NANDSEEWINDOW_H
