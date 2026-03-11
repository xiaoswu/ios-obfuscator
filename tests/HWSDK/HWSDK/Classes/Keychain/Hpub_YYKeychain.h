//
//  Hpub_YYKeychain.h
//  YYKit <https://github.com/ibireme/YYKit>
//
//  Created by ibireme on 14/10/15.
//  Copyright (c) 2015 ibireme.
//
//  This source code is licensed under the MIT-style license found in the
//  LICENSE file in the root directory of this source tree.
//

#import <Foundation/Foundation.h>

@class Hpub_YYKeychainItem;

NS_ASSUME_NONNULL_BEGIN

/**
 A wrapper for system keychain API.
 
 Inspired by [SSKeychain](https://github.com/soffes/sskeychain) 😜
 */
@interface Hpub_YYKeychain : NSObject

#pragma mark - Convenience method for keychain
///=============================================================================
/// @name Convenience method for keychain
///=============================================================================

/**
 Returns the password for a given account and service, or `nil` if not found or
 an error occurs.
 
 @param serviceName The service for which to return the corresponding password.
 This value must not be nil.
 
 @param account The account for which to return the corresponding password.
 This value must not be nil.
 
 @param error   On input, a pointer to an error object. If an error occurs,
 this pointer is set to an actual error object containing the error information. 
 You may specify nil for this parameter if you do not want the error information.
 See `Hpub_YYKeychainErrorCode`.
 
 @return Password string, or nil when not found or error occurs.
 */
+ (nullable NSString *)getPasswordForService:(NSString *)serviceName
                                     account:(NSString *)account
                                       error:(NSError **)error;
+ (nullable NSString *)getPasswordForService:(NSString *)serviceName
                                     account:(NSString *)account;

/**
 Deletes a password from the Keychain.
 
 @param serviceName The service for which to return the corresponding password.
 This value must not be nil.
 
 @param account The account for which to return the corresponding password.
 This value must not be nil.
 
 @param error   On input, a pointer to an error object. If an error occurs,
 this pointer is set to an actual error object containing the error information.
 You may specify nil for this parameter if you do not want the error information.
 See `Hpub_YYKeychainErrorCode`.
 
 @return Whether succeed.
 */
+ (BOOL)deletePasswordForService:(NSString *)serviceName account:(NSString *)account error:(NSError **)error;
+ (BOOL)deletePasswordForService:(NSString *)serviceName account:(NSString *)account;

/**
 Insert or update the password for a given account and service.
 
 @param password    The new password.
 
 @param serviceName The service for which to return the corresponding password.
 This value must not be nil.
 
 @param account The account for which to return the corresponding password.
 This value must not be nil.
 
 @param error   On input, a pointer to an error object. If an error occurs,
 this pointer is set to an actual error object containing the error information.
 You may specify nil for this parameter if you do not want the error information.
 See `Hpub_YYKeychainErrorCode`.
 
 @return Whether succeed.
 */
+ (BOOL)setPassword:(NSString *)password
         forService:(NSString *)serviceName
            account:(NSString *)account
              error:(NSError **)error;
+ (BOOL)setPassword:(NSString *)password
         forService:(NSString *)serviceName
            account:(NSString *)account;


#pragma mark - Full query for keychain (SQL-like)
///=============================================================================
/// @name Full query for keychain (SQL-like)
///=============================================================================

/**
 Insert an item into keychain.
 
 @discussion The service,account,password is required. If there's item exist
 already, an error occurs and insert fail.
 
 @param item  The item to insert.
 
 @param error On input, a pointer to an error object. If an error occurs,
 this pointer is set to an actual error object containing the error information.
 You may specify nil for this parameter if you do not want the error information.
 See `Hpub_YYKeychainErrorCode`.
 
 @return Whether succeed.
 */
+ (BOOL)insertItem:(Hpub_YYKeychainItem *)item error:(NSError **)error;
+ (BOOL)insertItem:(Hpub_YYKeychainItem *)item;

/**
 Update item in keychain.
 
 @discussion The service,account,password is required. If there's no item exist
 already, an error occurs and insert fail.
 
 @param item  The item to insert.
 
 @param error On input, a pointer to an error object. If an error occurs,
 this pointer is set to an actual error object containing the error information.
 You may specify nil for this parameter if you do not want the error information.
 See `Hpub_YYKeychainErrorCode`.
 
 @return Whether succeed.
 */
+ (BOOL)updateItem:(Hpub_YYKeychainItem *)item error:(NSError **)error;
+ (BOOL)updateItem:(Hpub_YYKeychainItem *)item;

/**
 Delete items from keychain.
 
 @discussion The service,account,password is required. If there's item exist
 already, an error occurs and insert fail.
 
 @param item  The item to update.
 
 @param error On input, a pointer to an error object. If an error occurs,
 this pointer is set to an actual error object containing the error information.
 You may specify nil for this parameter if you do not want the error information.
 See `Hpub_YYKeychainErrorCode`.
 
 @return Whether succeed.
 */
+ (BOOL)deleteItem:(Hpub_YYKeychainItem *)item error:(NSError **)error;
+ (BOOL)deleteItem:(Hpub_YYKeychainItem *)item;

/**
 Find an item from keychain.
 
 @discussion The service,account is optinal. It returns only one item if there
 exist multi.
 
 @param item  The item for query.
 
 @param error On input, a pointer to an error object. If an error occurs,
 this pointer is set to an actual error object containing the error information.
 You may specify nil for this parameter if you do not want the error information.
 See `Hpub_YYKeychainErrorCode`.
 
 @return An item or nil.
 */
+ (Hpub_YYKeychainItem *)selectOneItem:(Hpub_YYKeychainItem *)item error:(NSError **)error;
+ (Hpub_YYKeychainItem *)selectOneItem:(Hpub_YYKeychainItem *)item;

/**
 Find all items matches the query.
 
 @discussion The service,account is optinal. It returns all item matches by the
 query.
 
 @param item  The item for query.
 
 @param error On input, a pointer to an error object. If an error occurs,
 this pointer is set to an actual error object containing the error information.
 You may specify nil for this parameter if you do not want the error information.
 See `Hpub_YYKeychainErrorCode`.
 
 @return An array of Hpub_YYKeychainItem.
 */
+ (NSArray<Hpub_YYKeychainItem *> *)selectItems:(Hpub_YYKeychainItem *)item error:(NSError **)error;
+ (NSArray<Hpub_YYKeychainItem *> *)selectItems:(Hpub_YYKeychainItem *)item;

@end




#pragma mark - Const

/**
 Error code in Hpub_YYKeychain API.
 */
typedef NS_ENUM (NSUInteger, Hpub_YYKeychainErrorCode) {
    Hpub_YYKeychainErrorUnimplemented = 1, ///< Function or operation not implemented.
    Hpub_YYKeychainErrorIO, ///< I/O error (bummers)
    Hpub_YYKeychainErrorOpWr, ///< File already open with with write permission.
    Hpub_YYKeychainErrorParam, ///< One or more parameters passed to a function where not valid.
    Hpub_YYKeychainErrorAllocate, ///< Failed to allocate memory.
    Hpub_YYKeychainErrorUserCancelled, ///< User cancelled the operation.
    Hpub_YYKeychainErrorBadReq, ///< Bad parameter or invalid state for operation.
    Hpub_YYKeychainErrorInternalComponent, ///< Internal...
    Hpub_YYKeychainErrorNotAvailable, ///< No keychain is available. You may need to restart your computer.
    Hpub_YYKeychainErrorDuplicateItem, ///< The specified item already exists in the keychain.
    Hpub_YYKeychainErrorItemNotFound, ///< The specified item could not be found in the keychain.
    Hpub_YYKeychainErrorInteractionNotAllowed, ///< User interaction is not allowed.
    Hpub_YYKeychainErrorDecode, ///< Unable to decode the provided data.
    Hpub_YYKeychainErrorAuthFailed, ///< The user name or passphrase you entered is not.
};


/**
 When query to return the item's data, the error
 errSecInteractionNotAllowed will be returned if the item's data is not
 available until a device unlock occurs.
 */
typedef NS_ENUM (NSUInteger, Hpub_YYKeychainAccessible) {
    Hpub_YYKeychainAccessibleNone = 0, ///< no value
    
    /** Item data can only be accessed
     while the device is unlocked. This is recommended for items that only
     need be accesible while the application is in the foreground.  Items
     with this attribute will migrate to a new device when using encrypted
     backups. */
    Hpub_YYKeychainAccessibleWhenUnlocked,
    
    /** Item data can only be
     accessed once the device has been unlocked after a restart.  This is
     recommended for items that need to be accesible by background
     applications. Items with this attribute will migrate to a new device
     when using encrypted backups.*/
    Hpub_YYKeychainAccessibleAfterFirstUnlock,
    
    /** Item data can always be accessed
     regardless of the lock state of the device.  This is not recommended
     for anything except system use. Items with this attribute will migrate
     to a new device when using encrypted backups.*/
    Hpub_YYKeychainAccessibleAlways,
    
    /** Item data can
     only be accessed while the device is unlocked. This class is only
     available if a passcode is set on the device. This is recommended for
     items that only need to be accessible while the application is in the
     foreground. Items with this attribute will never migrate to a new
     device, so after a backup is restored to a new device, these items
     will be missing. No items can be stored in this class on devices
     without a passcode. Disabling the device passcode will cause all
     items in this class to be deleted.*/
    Hpub_YYKeychainAccessibleWhenPasscodeSetThisDeviceOnly,
    
    /** Item data can only
     be accessed while the device is unlocked. This is recommended for items
     that only need be accesible while the application is in the foreground.
     Items with this attribute will never migrate to a new device, so after
     a backup is restored to a new device, these items will be missing. */
    Hpub_YYKeychainAccessibleWhenUnlockedThisDeviceOnly,
    
    /** Item data can
     only be accessed once the device has been unlocked after a restart.
     This is recommended for items that need to be accessible by background
     applications. Items with this attribute will never migrate to a new
     device, so after a backup is restored to a new device these items will
     be missing.*/
    Hpub_YYKeychainAccessibleAfterFirstUnlockThisDeviceOnly,
    
    /** Item data can always
     be accessed regardless of the lock state of the device.  This option
     is not recommended for anything except system use. Items with this
     attribute will never migrate to a new device, so after a backup is
     restored to a new device, these items will be missing.*/
    Hpub_YYKeychainAccessibleAlwaysThisDeviceOnly,
};

/**
 Whether the item in question can be synchronized.
 */
typedef NS_ENUM (NSUInteger, Hpub_YYKeychainQuerySynchronizationMode) {
    
    /** Default, Don't care for synchronization  */
    Hpub_YYKeychainQuerySynchronizationModeAny = 0,
    
    /** Is not synchronized */
    Hpub_YYKeychainQuerySynchronizationModeNo,
    
    /** To add a new item which can be synced to other devices, or to obtain 
     synchronized results from a query*/
    Hpub_YYKeychainQuerySynchronizationModeYes,
} NS_AVAILABLE_IOS (7_0);


#pragma mark - Item

/**
 Wrapper for keychain item/query.
 */
@interface Hpub_YYKeychainItem : NSObject <NSCopying>

@property (nullable, nonatomic, copy) NSString *service; ///< kSecAttrService
@property (nullable, nonatomic, copy) NSString *account; ///< kSecAttrAccount
@property (nullable, nonatomic, copy) NSData *passwordData; ///< kSecValueData
@property (nullable, nonatomic, copy) NSString *password; ///< shortcut for `passwordData`
@property (nullable, nonatomic, copy) id <NSCoding> passwordObject; ///< shortcut for `passwordData`

@property (nullable, nonatomic, copy) NSString *label; ///< kSecAttrLabel
@property (nullable, nonatomic, copy) NSNumber *type; ///< kSecAttrType (FourCC)
@property (nullable, nonatomic, copy) NSNumber *creater; ///< kSecAttrCreator (FourCC)
@property (nullable, nonatomic, copy) NSString *comment; ///< kSecAttrComment
@property (nullable, nonatomic, copy) NSString *descr; ///< kSecAttrDescription
@property (nullable, nonatomic, readonly, strong) NSDate *modificationDate; ///< kSecAttrModificationDate
@property (nullable, nonatomic, readonly, strong) NSDate *creationDate; ///< kSecAttrCreationDate
@property (nullable, nonatomic, copy) NSString *accessGroup; ///< kSecAttrAccessGroup

@property (nonatomic) Hpub_YYKeychainAccessible accessible; ///< kSecAttrAccessible
@property (nonatomic) Hpub_YYKeychainQuerySynchronizationMode synchronizable NS_AVAILABLE_IOS(7_0); ///< kSecAttrSynchronizable

@end

NS_ASSUME_NONNULL_END
