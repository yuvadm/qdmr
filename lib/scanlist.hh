#ifndef SCANLIST_HH
#define SCANLIST_HH

#include <QObject>
#include <QAbstractListModel>

#include "channel.hh"

/** Generic representation of a scan list.
 * @ingroup conf */
class ScanList : public QAbstractListModel
{
	Q_OBJECT

public:
  /** Constructs a scan list with the given name. */
	ScanList(const QString &name, QObject *parent=nullptr);

  /** Returns the number of channels within the scanlist. */
	int count() const;
  /** Clears the scan list. */
	void clear();
  /** Returns the name of the scanlist. */
	const QString &name() const;
  /** Sets the name of the scanlist. */
	bool setName(const QString &name);

  /** Returns @c true if the given channel is part of this scanlist. */
  bool contains(Channel *channel) const;
  /** Returns the channel at the given index. */
	Channel *channel(int idx) const;
  /** Adds a channel to the scan list. */
	bool addChannel(Channel *channel);
  /** Removes the channel at the given index. */
	bool remChannel(int idx);
  /** Removes the given channel. */
	bool remChannel(Channel *channel);

  /** Returns the priority channel. */
  Channel *priorityChannel() const;
  /** Sets the priority channel. */
  void setPriorityChannel(Channel *channel);
  /** Returns the secondary priority channel. */
  Channel *secPriorityChannel() const;
  /** Sets the secondary priority channel. */
  void setSecPriorityChannel(Channel *channel);

	/** Implementation of QAbstractListModel, returns the number of channels. */
	int rowCount(const QModelIndex &parent = QModelIndex()) const;
  /** Implementation of QAbstractListModel, returns the entry data at the given index. */
	QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
  /** Implementation of QAbstractListModel, returns the header data at the given section. */
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

signals:
  /** Gets emitted whenever the scanlist is modified. */
	void modified();

protected slots:
  /** Internal used callback to handle deleted channels. */
	void onChannelDeleted(QObject *obj);

protected:
  /** The scanlist name. */
	QString _name;
  /** The channel list. */
	QVector<Channel *> _channels;
  /** The priority channel. */
  Channel *_priorityChannel;
  /** The secondary priority channel. */
  Channel *_secPriorityChannel;
};



/** Represents the list of scan lists.
 * @ingroup conf */
class ScanLists: public QAbstractListModel
{
	Q_OBJECT

public:
  /** Constructs an empty list. */
	explicit ScanLists(QObject *parent = nullptr);

  /** Returns the number of scanlists. */
	int count() const;
  /** Returns the index of the scanlist. */
  int indexOf(ScanList *list) const;
  /** Clears the list. */
	void clear();

  /** Returns the scanlist at the given index. */
	ScanList *scanlist(int idx) const;
  /** Adds a scan list at the given position, if row<0 the scanlist gets appended. */
	bool addScanList(ScanList *list, int row=-1);
  /** Removes the scanlist at the given position. */
	bool remScanList(int idx);
  /** Removes the specified scanlist from the list. */
	bool remScanList(ScanList *list);

  /** Moves the scanlist at the given row one up. */
  bool moveUp(int row);
  /** Moves the scanlist at the given row one down. */
  bool moveDown(int row);

	/** Implementation of QAbstractListModel, returns the number of scanlists. */
	int rowCount(const QModelIndex &parent = QModelIndex()) const;
  /** Implementation of QAbstractListModel, returns the item data at the given index. */
	QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
  /** Implementation of QAbstractListModel, returns the header data at the given section. */
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

signals:
  /** Gets modified whenever the list of any of the scanlists gets modified. */
	void modified();

protected slots:
  /** Internal callback to handle deleted scanlists. */
	void onScanListDeleted(QObject *obj);

protected:
  /** The list of scanlists. */
	QVector<ScanList *> _scanlists;
};


#endif // SCANLIST_HH
